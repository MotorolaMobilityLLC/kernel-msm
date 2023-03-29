/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * This file parser_api.cc contains the code for parsing
 * 802.11 messages.
 * Author:        Pierre Vandwalle
 * Date:          03/18/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#include "sir_api.h"
#include "ani_global.h"
#include "parser_api.h"
#include "lim_utils.h"
#include "utils_parser.h"
#include "lim_ser_des_utils.h"
#include "sch_api.h"
#include "wmm_apsd.h"
#include "rrm_api.h"

#include "cds_regdomain.h"
#include "qdf_crypto.h"
#include "lim_process_fils.h"
#include "wlan_utility.h"
#include "wifi_pos_api.h"
#include "wlan_mlme_public_struct.h"
#include "wlan_mlme_ucfg_api.h"
#include "wlan_mlme_api.h"
#include "wlan_reg_services_api.h"
#include "wlan_cm_roam_api.h"

#define RSN_OUI_SIZE 4
/* ////////////////////////////////////////////////////////////////////// */
void swap_bit_field16(uint16_t in, uint16_t *out)
{
#ifdef ANI_LITTLE_BIT_ENDIAN
	*out = in;
#else                           /* Big-Endian... */
	*out = ((in & 0x8000) >> 15) |
	       ((in & 0x4000) >> 13) |
	       ((in & 0x2000) >> 11) |
	       ((in & 0x1000) >> 9) |
	       ((in & 0x0800) >> 7) |
	       ((in & 0x0400) >> 5) |
	       ((in & 0x0200) >> 3) |
	       ((in & 0x0100) >> 1) |
	       ((in & 0x0080) << 1) |
	       ((in & 0x0040) << 3) |
	       ((in & 0x0020) << 5) |
	       ((in & 0x0010) << 7) |
	       ((in & 0x0008) << 9) |
	       ((in & 0x0004) << 11) |
	       ((in & 0x0002) << 13) | ((in & 0x0001) << 15);
#endif /* ANI_LITTLE_BIT_ENDIAN */
}

static inline void __print_wmm_params(struct mac_context *mac,
				      tDot11fIEWMMParams *pWmm)
{
	pe_debug("WMM Parameters Received:");
	pe_debug("BE: aifsn %d, acm %d, aci %d, cwmin %d, cwmax %d, txop %d",
		pWmm->acbe_aifsn, pWmm->acbe_acm, pWmm->acbe_aci,
		pWmm->acbe_acwmin, pWmm->acbe_acwmax, pWmm->acbe_txoplimit);

	pe_debug("BK: aifsn %d, acm %d, aci %d, cwmin %d, cwmax %d, txop %d",
		pWmm->acbk_aifsn, pWmm->acbk_acm, pWmm->acbk_aci,
		pWmm->acbk_acwmin, pWmm->acbk_acwmax, pWmm->acbk_txoplimit);

	pe_debug("VI: aifsn %d, acm %d, aci %d, cwmin %d, cwmax %d, txop %d",
		pWmm->acvi_aifsn, pWmm->acvi_acm, pWmm->acvi_aci,
		pWmm->acvi_acwmin, pWmm->acvi_acwmax, pWmm->acvi_txoplimit);

	pe_debug("VO: aifsn %d, acm %d, aci %d, cwmin %d, cwmax %d, txop %d",
		pWmm->acvo_aifsn, pWmm->acvo_acm, pWmm->acvo_aci,
		pWmm->acvo_acwmin, pWmm->acvo_acwmax, pWmm->acvo_txoplimit);

	return;
}

/* ////////////////////////////////////////////////////////////////////// */
/* Functions for populating "dot11f" style IEs */

/* return: >= 0, the starting location of the IE in rsnIEdata inside tSirRSNie */
/*         < 0, cannot find */
int find_ie_location(struct mac_context *mac, tpSirRSNie pRsnIe, uint8_t EID)
{
	int idx, ieLen, bytesLeft;
	int ret_val = -1;

	/* Here's what's going on: 'rsnIe' looks like this: */

	/*     typedef struct sSirRSNie */
	/*     { */
	/*         uint16_t       length; */
	/*         uint8_t        rsnIEdata[WLAN_MAX_IE_LEN+2]; */
	/*     } tSirRSNie, *tpSirRSNie; */

	/* other code records both the WPA & RSN IEs (including their EIDs & */
	/* lengths) into the array 'rsnIEdata'.  We may have: */

	/*     With WAPI support, there may be 3 IEs here */
	/*     It can be only WPA IE, or only RSN IE or only WAPI IE */
	/*     Or two or all three of them with no particular ordering */

	/* The if/then/else statements that follow are here to figure out */
	/* whether we have the WPA IE, and where it is if we *do* have it. */

	/* Save the first IE length */
	ieLen = pRsnIe->rsnIEdata[1] + 2;
	idx = 0;
	bytesLeft = pRsnIe->length;

	while (1) {
		if (EID == pRsnIe->rsnIEdata[idx])
			/* Found it */
			return idx;
		if (EID != pRsnIe->rsnIEdata[idx] &&
		    /* & if no more IE, */
		    bytesLeft <= (uint16_t)(ieLen))
			return ret_val;

		bytesLeft -= ieLen;
		ieLen = pRsnIe->rsnIEdata[idx + 1] + 2;
		idx += ieLen;
	}

	return ret_val;
}

QDF_STATUS
populate_dot11f_capabilities(struct mac_context *mac,
			     tDot11fFfCapabilities *pDot11f,
			     struct pe_session *pe_session)
{
	uint16_t cfg;
	QDF_STATUS nSirStatus;

	nSirStatus = lim_get_capability_info(mac, &cfg, pe_session);
	if (QDF_STATUS_SUCCESS != nSirStatus) {
		pe_err("Failed to retrieve the Capabilities bitfield from CFG status: %d",
			   nSirStatus);
		return nSirStatus;
	}

	swap_bit_field16(cfg, (uint16_t *) pDot11f);

	return QDF_STATUS_SUCCESS;
} /* End populate_dot11f_capabilities. */

/**
 * populate_dot_11_f_ext_chann_switch_ann() - Function to populate ECS
 * @mac_ptr:            Pointer to PMAC structure
 * @dot_11_ptr:         ECS element
 * @session_entry:      PE session entry
 *
 * This function is used to populate the extended channel switch element
 *
 * Return: None
 */
void populate_dot_11_f_ext_chann_switch_ann(struct mac_context *mac_ptr,
		tDot11fIEext_chan_switch_ann *dot_11_ptr,
		struct pe_session *session_entry)
{
	uint8_t ch_offset;
	uint32_t sw_target_freq;
	uint8_t primary_channel;
	enum phy_ch_width ch_width;

	ch_width = session_entry->gLimChannelSwitch.ch_width;
	ch_offset = session_entry->gLimChannelSwitch.sec_ch_offset;

	dot_11_ptr->switch_mode = session_entry->gLimChannelSwitch.switchMode;
	sw_target_freq = session_entry->gLimChannelSwitch.sw_target_freq;
	primary_channel = session_entry->gLimChannelSwitch.primaryChannel;
	dot_11_ptr->new_reg_class =
		lim_op_class_from_bandwidth(mac_ptr, sw_target_freq, ch_width,
					    ch_offset);
	dot_11_ptr->new_channel =
		session_entry->gLimChannelSwitch.primaryChannel;
	dot_11_ptr->switch_count =
		session_entry->gLimChannelSwitch.switchCount;
	dot_11_ptr->present = 1;

	pe_debug("country:%s chan:%d freq %d width:%d reg:%d off:%d",
		 mac_ptr->scan.countryCodeCurrent,
		 session_entry->gLimChannelSwitch.primaryChannel,
		 sw_target_freq,
		 session_entry->gLimChannelSwitch.ch_width,
		 dot_11_ptr->new_reg_class,
		 session_entry->gLimChannelSwitch.sec_ch_offset);
}

void
populate_dot11f_chan_switch_ann(struct mac_context *mac,
				tDot11fIEChanSwitchAnn *pDot11f,
				struct pe_session *pe_session)
{
	pDot11f->switchMode = pe_session->gLimChannelSwitch.switchMode;
	pDot11f->newChannel = pe_session->gLimChannelSwitch.primaryChannel;
	pDot11f->switchCount =
		(uint8_t) pe_session->gLimChannelSwitch.switchCount;

	pDot11f->present = 1;
}

/**
 * populate_dot11_supp_operating_classes() - Function to populate supported
 *                      operating class IE
 * @mac_ptr:            Pointer to PMAC structure
 * @dot_11_ptr:         Operating class element
 * @session_entry:      PE session entry
 *
 * Return: None
 */
void
populate_dot11_supp_operating_classes(struct mac_context *mac_ptr,
				tDot11fIESuppOperatingClasses *dot_11_ptr,
				struct pe_session *session_entry)
{
	uint8_t ch_offset;

	if (session_entry->ch_width == CH_WIDTH_80MHZ) {
		ch_offset = BW80;
	} else {
		switch (session_entry->htSecondaryChannelOffset) {
		case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
			ch_offset = BW40_HIGH_PRIMARY;
			break;
		case PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
			ch_offset = BW40_LOW_PRIMARY;
			break;
		default:
			ch_offset = BW20;
			break;
		}
	}

	wlan_reg_dmn_get_curr_opclasses(&dot_11_ptr->num_classes,
					&dot_11_ptr->classes[1]);
	dot_11_ptr->classes[0] =
		lim_op_class_from_bandwidth(mac_ptr,
					    session_entry->curr_op_freq,
					    session_entry->ch_width,
					    ch_offset);
	dot_11_ptr->num_classes++;
	dot_11_ptr->present = 1;
}

void
populate_dot11f_tx_power_env(struct mac_context *mac,
			     tDot11fIEtransmit_power_env *tpe_ptr,
			     enum phy_ch_width chan_width, uint32_t chan_freq,
			     uint16_t *num_tpe, bool is_chan_switch)
{
	uint8_t count;
	uint16_t eirp_power, reg_power;
	int power_for_bss;
	bool add_eirp_power = false;
	struct ch_params chan_params;
	bool psd_tpe = false;
	uint32_t bw_threshold, bw_val;
	int num_tpe_ies = 0;
	uint32_t num_tx_power, num_tx_power_psd;
	uint32_t max_tx_pwr_count, max_tx_pwr_count_psd;
	qdf_freq_t psd_start_freq;

	if (!wlan_reg_is_6ghz_chan_freq(chan_freq)) {
		psd_tpe = false;
	} else {
		wlan_reg_get_client_power_for_6ghz_ap(mac->pdev,
						      REG_DEFAULT_CLIENT,
						      chan_freq,
						      &psd_tpe,
						      &reg_power, &eirp_power);
		pe_debug("chan_freq %d, reg_power %d, psd_power %d",
			 chan_freq, reg_power, eirp_power);
	}

	switch (chan_width) {
	case CH_WIDTH_20MHZ:
		max_tx_pwr_count = 0;
		max_tx_pwr_count_psd = 1;
		num_tx_power = 1;
		num_tx_power_psd = 1;
		break;

	case CH_WIDTH_40MHZ:
		max_tx_pwr_count = 1;
		max_tx_pwr_count_psd = 2;
		num_tx_power = 2;
		num_tx_power_psd = 2;
		break;

	case CH_WIDTH_80MHZ:
		max_tx_pwr_count = 2;
		max_tx_pwr_count_psd = 3;
		num_tx_power = 3;
		num_tx_power_psd = 4;
		break;

	case CH_WIDTH_160MHZ:
	case CH_WIDTH_80P80MHZ:
		max_tx_pwr_count = 3;
		max_tx_pwr_count_psd = 4;
		num_tx_power = 4;
		num_tx_power_psd = 8;
		break;
	default:
		return;
	}

	if (!psd_tpe) {
		reg_power = wlan_reg_get_channel_reg_power_for_freq(
			mac->pdev, chan_freq);

		tpe_ptr->present = 1;
		tpe_ptr->max_tx_pwr_count = max_tx_pwr_count;
		tpe_ptr->max_tx_pwr_interpret = 0;
		tpe_ptr->max_tx_pwr_category = 0;
		tpe_ptr->num_tx_power = num_tx_power;
		for (count = 0; count < num_tx_power; count++)
			tpe_ptr->tx_power[count] = reg_power;

		num_tpe_ies++;
		tpe_ptr++;
	} else {

		bw_val = wlan_reg_get_bw_value(chan_width);
		bw_threshold = 20;
		power_for_bss = eirp_power + 13;

		while ((reg_power > power_for_bss) &&
		       (bw_threshold < bw_val)) {
			bw_threshold = 2 * bw_threshold;
			power_for_bss += 3;
		}
		if (bw_threshold < bw_val)
			add_eirp_power = true;

		pe_debug("bw_threshold %d", bw_threshold);

		if (add_eirp_power) {
			tpe_ptr->present = 1;
			tpe_ptr->max_tx_pwr_count = max_tx_pwr_count;
			tpe_ptr->max_tx_pwr_interpret = 2;
			tpe_ptr->max_tx_pwr_category = 0;
			tpe_ptr->num_tx_power = num_tx_power;
			for (count = 0; count < num_tx_power; count++) {
				tpe_ptr->tx_power[count] = reg_power * 2;
				pe_debug("non-psd default TPE %d %d",
					 count, tpe_ptr->tx_power[count]);
			}
			num_tpe_ies++;
			tpe_ptr++;
		}

		wlan_reg_get_client_power_for_6ghz_ap(mac->pdev,
						      REG_SUBORDINATE_CLIENT,
						      chan_freq,
						      &psd_tpe,
						      &reg_power,
						      &eirp_power);

		if (reg_power) {
			bw_val = wlan_reg_get_bw_value(chan_width);
			bw_threshold = 20;
			power_for_bss = eirp_power + 13;

			while ((reg_power > power_for_bss) &&
			       (bw_threshold < bw_val)) {
				bw_threshold = 2 * bw_threshold;
				power_for_bss += 3;
			}
			if (bw_threshold < bw_val)
				add_eirp_power = true;

			if (add_eirp_power) {
				tpe_ptr->present = 1;
				tpe_ptr->max_tx_pwr_count = max_tx_pwr_count;
				tpe_ptr->max_tx_pwr_interpret = 2;
				tpe_ptr->max_tx_pwr_category = 1;
				tpe_ptr->num_tx_power = num_tx_power;
				for (count = 0; count < num_tx_power; count++) {
					tpe_ptr->tx_power[count] =
							reg_power * 2;
					pe_debug("non-psd subord TPE %d %d",
						 count,
						 tpe_ptr->tx_power[count]);
				}
				num_tpe_ies++;
				tpe_ptr++;
			}
		}

		tpe_ptr->present = 1;
		tpe_ptr->max_tx_pwr_count = max_tx_pwr_count_psd;
		tpe_ptr->max_tx_pwr_interpret = 3;
		tpe_ptr->max_tx_pwr_category = 0;
		tpe_ptr->num_tx_power = num_tx_power_psd;

		chan_params.ch_width = chan_width;
		bw_val = wlan_reg_get_bw_value(chan_width);
		wlan_reg_set_channel_params_for_freq(mac->pdev, chan_freq,
						     chan_freq,
						     &chan_params);

		if (chan_params.mhz_freq_seg1)
			psd_start_freq =
				chan_params.mhz_freq_seg1 - bw_val / 2 + 10;
		else
			psd_start_freq =
				chan_params.mhz_freq_seg0 - bw_val / 2 + 10;

		for (count = 0; count < num_tx_power_psd; count++) {
			wlan_reg_get_client_power_for_6ghz_ap(
							mac->pdev,
							REG_DEFAULT_CLIENT,
							psd_start_freq +
							20 * count,
							&psd_tpe,
							&reg_power,
							&eirp_power);
			tpe_ptr->tx_power[count] = eirp_power * 2;
			pe_debug("psd default TPE %d %d",
				 count, tpe_ptr->tx_power[count]);
		}
		num_tpe_ies++;
		tpe_ptr++;

		wlan_reg_get_client_power_for_6ghz_ap(mac->pdev,
						      REG_SUBORDINATE_CLIENT,
						      chan_freq,
						      &psd_tpe,
						      &reg_power,
						      &eirp_power);

		if (eirp_power) {
			tpe_ptr->present = 1;
			tpe_ptr->max_tx_pwr_count = max_tx_pwr_count_psd;
			tpe_ptr->max_tx_pwr_interpret = 3;
			tpe_ptr->max_tx_pwr_category = 1;
			tpe_ptr->num_tx_power = num_tx_power_psd;

			for (count = 0; count < num_tx_power_psd; count++) {
				wlan_reg_get_client_power_for_6ghz_ap(
							mac->pdev,
							REG_SUBORDINATE_CLIENT,
							psd_start_freq +
							20 * count,
							&psd_tpe,
							&reg_power,
							&eirp_power);
				tpe_ptr->tx_power[count] = eirp_power * 2;
				pe_debug("psd subord TPE %d %d",
					 count, tpe_ptr->tx_power[count]);
			}
			num_tpe_ies++;
			tpe_ptr++;
		}
	}
	*num_tpe = num_tpe_ies;
}

void
populate_dot11f_chan_switch_wrapper(struct mac_context *mac,
				    tDot11fIEChannelSwitchWrapper *pDot11f,
				    struct pe_session *pe_session)
{
	uint16_t num_tpe;
	/*
	 * The new country subelement is present only when
	 * 1. AP performs Extended Channel switching to new country.
	 * 2. New Operating Class table or a changed set of operating
	 * classes relative to the contents of the country element sent
	 * in the beacons.
	 *
	 * In the current scenario Channel Switch wrapper IE is included
	 * when we a radar is found and the AP does a channel change in
	 * the same regulatory domain(No country change or Operating class
	 * table). So, we do not need to include the New Country IE.
	 *
	 * Transmit Power Envlope Subelement is optional
	 * in Channel Switch Wrapper IE. So, not setting
	 * the TPE subelement. We include only WiderBWChanSwitchAnn.
	 */
	pDot11f->present = 1;

	/*
	 * Add the Wide Channel Bandwidth Sublement.
	 */
	pDot11f->WiderBWChanSwitchAnn.newChanWidth =
		pe_session->gLimWiderBWChannelSwitch.newChanWidth;
	pDot11f->WiderBWChanSwitchAnn.newCenterChanFreq0 =
		pe_session->gLimWiderBWChannelSwitch.newCenterChanFreq0;
	pDot11f->WiderBWChanSwitchAnn.newCenterChanFreq1 =
		pe_session->gLimWiderBWChannelSwitch.newCenterChanFreq1;
	pDot11f->WiderBWChanSwitchAnn.present = 1;

	/*
	 * Add the Transmit power Envelope Sublement.
	 */
	if (pe_session->vhtCapability) {
		populate_dot11f_tx_power_env(mac,
				&pDot11f->transmit_power_env,
				pe_session->gLimChannelSwitch.ch_width,
				pe_session->gLimChannelSwitch.sw_target_freq,
				&num_tpe, true);
	}
}

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
void
populate_dot11f_avoid_channel_ie(struct mac_context *mac_ctx,
				 tDot11fIEQComVendorIE *dot11f,
				 struct pe_session *pe_session)
{
	if (!pe_session->sap_advertise_avoid_ch_ie)
		return;

	dot11f->present = true;
	dot11f->type = QCOM_VENDOR_IE_MCC_AVOID_CH;
	dot11f->channel = wlan_reg_freq_to_chan(
		mac_ctx->pdev, pe_session->curr_op_freq);
}
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */

QDF_STATUS
populate_dot11f_country(struct mac_context *mac,
			tDot11fIECountry *ctry_ie, struct pe_session *pe_session)
{
	uint8_t code[REG_ALPHA2_LEN + 1];
	uint8_t cur_triplet_num_chans = 0;
	int chan_enum, chan_num, chan_spacing = 0;
	struct regulatory_channel *sec_cur_chan_list;
	struct regulatory_channel *cur_chan, *start, *prev;
	enum reg_wifi_band rf_band = REG_BAND_UNKNOWN;
	uint8_t buffer_triplets[81][3];
	uint8_t i, j, num_triplets = 0;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	bool six_gig_started = false;

	sec_cur_chan_list = qdf_mem_malloc(NUM_CHANNELS *
					   sizeof(*sec_cur_chan_list));
	if (!sec_cur_chan_list)
		return QDF_STATUS_E_NOMEM;

	lim_get_rf_band_new(mac, &rf_band, pe_session);
	switch (rf_band) {
	case REG_BAND_2G:
		chan_spacing = 1;
		break;
	case REG_BAND_5G:
	case REG_BAND_6G:
		chan_spacing = 4;
		break;
	case REG_BAND_UNKNOWN:
		pe_err("Wrong reg band for country info");
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}

	chan_num = wlan_reg_get_secondary_band_channel_list(mac->pdev,
							    BIT(rf_band),
							    sec_cur_chan_list);
	if (!chan_num) {
		pe_err("failed to get cur_chan list");
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}

	wlan_reg_read_current_country(mac->psoc, code);
	qdf_mem_copy(ctry_ie->country, code, REG_ALPHA2_LEN);

	/* advertise global operating class */
	ctry_ie->country[REG_ALPHA2_LEN] = 0x04;

	start = NULL;
	prev = NULL;
	for (chan_enum = 0; chan_enum < chan_num; chan_enum++) {
		cur_chan = &sec_cur_chan_list[chan_enum];

		if (cur_chan->chan_flags & REGULATORY_CHAN_DISABLED)
			continue;

		if (wlan_reg_is_6ghz_chan_freq(cur_chan->center_freq) &&
		    !six_gig_started) {
			buffer_triplets[num_triplets][0] = OP_CLASS_ID_201;
			buffer_triplets[num_triplets][1] = OP_CLASS_131;
			num_triplets++;
			six_gig_started = true;
		}

		if (start && prev &&
		    prev->chan_num + chan_spacing == cur_chan->chan_num &&
		    start->tx_power == cur_chan->tx_power) {
			/* Can use same entry */
			prev = cur_chan;
			cur_triplet_num_chans++;
			continue;
		}

		if (start && prev) {
			/* Save as entry */
			buffer_triplets[num_triplets][0] = start->chan_num;
			buffer_triplets[num_triplets][1] =
					cur_triplet_num_chans + 1;
			buffer_triplets[num_triplets][2] = start->tx_power;
			start = NULL;
			cur_triplet_num_chans = 0;

			num_triplets++;
			if (num_triplets > 80) {
				pe_err("Triplets number exceed max size");
				status = QDF_STATUS_E_FAILURE;
				goto out;
			}
		}

		if ((chan_enum == NUM_CHANNELS - 1) && (six_gig_started)) {
			buffer_triplets[num_triplets][0] = OP_CLASS_ID_201;
			buffer_triplets[num_triplets][1] = OP_CLASS_132;
			num_triplets++;

			buffer_triplets[num_triplets][0] = OP_CLASS_ID_201;
			buffer_triplets[num_triplets][1] = OP_CLASS_133;
			num_triplets++;

			buffer_triplets[num_triplets][0] = OP_CLASS_ID_201;
			buffer_triplets[num_triplets][1] = OP_CLASS_134;
			num_triplets++;
		}

		/* Start new group */
		start = cur_chan;
		prev = cur_chan;
	}

	if (start) {
		buffer_triplets[num_triplets][0] = start->chan_num;
		buffer_triplets[num_triplets][1] = cur_triplet_num_chans + 1;
		buffer_triplets[num_triplets][2] = start->tx_power;
		num_triplets++;
	}

	if (!num_triplets) {
		/* at-least one triplet should be present */
		pe_err("No triplet present");
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}

	ctry_ie->num_more_triplets = num_triplets - 1;
	ctry_ie->first_triplet[0] = buffer_triplets[0][0];
	ctry_ie->first_triplet[1] = buffer_triplets[0][1];
	ctry_ie->first_triplet[2] = buffer_triplets[0][2];

	for (i = 0; i < ctry_ie->num_more_triplets; i++) {
		for (j = 0; j < 3; j++)
			ctry_ie->more_triplets[i][j] =
					buffer_triplets[i + 1][j];
	}
	ctry_ie->present = 1;

out:
	qdf_mem_free(sec_cur_chan_list);
	return status;
} /* End populate_dot11f_country. */

/**
 * populate_dot11f_ds_params() - To populate DS IE params
 * mac_ctx: Pointer to global mac context
 * dot11f_param: pointer to DS params IE
 * freq: freq
 *
 * This routine will populate DS param in management frame like
 * beacon, probe response, and etc.
 *
 * Return: Overall success
 */
QDF_STATUS
populate_dot11f_ds_params(struct mac_context *mac_ctx,
			  tDot11fIEDSParams *dot11f_param, qdf_freq_t freq)
{
	if (WLAN_REG_IS_24GHZ_CH_FREQ(freq)) {
		/* .11b/g mode PHY => Include the DS Parameter Set IE: */
		dot11f_param->curr_channel = wlan_reg_freq_to_chan(
								mac_ctx->pdev,
								freq);
		dot11f_param->present = 1;
	}

	return QDF_STATUS_SUCCESS;
}

#define SET_AIFSN(aifsn) (((aifsn) < 2) ? 2 : (aifsn))

void
populate_dot11f_edca_param_set(struct mac_context *mac,
			       tDot11fIEEDCAParamSet *pDot11f,
			       struct pe_session *pe_session)
{

	if (pe_session->limQosEnabled) {
		/* change to bitwise operation, after this is fixed in frames. */
		pDot11f->qos =
			(uint8_t) (0xf0 &
				   (pe_session->gLimEdcaParamSetCount << 4));

		/* Fill each EDCA parameter set in order: be, bk, vi, vo */
		pDot11f->acbe_aifsn =
			(0xf &
			 SET_AIFSN(pe_session->gLimEdcaParamsBC[0].aci.aifsn));
		pDot11f->acbe_acm =
			(0x1 & pe_session->gLimEdcaParamsBC[0].aci.acm);
		pDot11f->acbe_aci = (0x3 & QCA_WLAN_AC_BE);
		pDot11f->acbe_acwmin =
			(0xf & pe_session->gLimEdcaParamsBC[0].cw.min);
		pDot11f->acbe_acwmax =
			(0xf & pe_session->gLimEdcaParamsBC[0].cw.max);
		pDot11f->acbe_txoplimit =
			pe_session->gLimEdcaParamsBC[0].txoplimit;

		pDot11f->acbk_aifsn =
			(0xf &
			 SET_AIFSN(pe_session->gLimEdcaParamsBC[1].aci.aifsn));
		pDot11f->acbk_acm =
			(0x1 & pe_session->gLimEdcaParamsBC[1].aci.acm);
		pDot11f->acbk_aci = (0x3 & QCA_WLAN_AC_BK);
		pDot11f->acbk_acwmin =
			(0xf & pe_session->gLimEdcaParamsBC[1].cw.min);
		pDot11f->acbk_acwmax =
			(0xf & pe_session->gLimEdcaParamsBC[1].cw.max);
		pDot11f->acbk_txoplimit =
			pe_session->gLimEdcaParamsBC[1].txoplimit;

		pDot11f->acvi_aifsn =
			(0xf &
			 SET_AIFSN(pe_session->gLimEdcaParamsBC[2].aci.aifsn));
		pDot11f->acvi_acm =
			(0x1 & pe_session->gLimEdcaParamsBC[2].aci.acm);
		pDot11f->acvi_aci = (0x3 & QCA_WLAN_AC_VI);
		pDot11f->acvi_acwmin =
			(0xf & pe_session->gLimEdcaParamsBC[2].cw.min);
		pDot11f->acvi_acwmax =
			(0xf & pe_session->gLimEdcaParamsBC[2].cw.max);
		pDot11f->acvi_txoplimit =
			pe_session->gLimEdcaParamsBC[2].txoplimit;

		pDot11f->acvo_aifsn =
			(0xf &
			 SET_AIFSN(pe_session->gLimEdcaParamsBC[3].aci.aifsn));
		pDot11f->acvo_acm =
			(0x1 & pe_session->gLimEdcaParamsBC[3].aci.acm);
		pDot11f->acvo_aci = (0x3 & QCA_WLAN_AC_VO);
		pDot11f->acvo_acwmin =
			(0xf & pe_session->gLimEdcaParamsBC[3].cw.min);
		pDot11f->acvo_acwmax =
			(0xf & pe_session->gLimEdcaParamsBC[3].cw.max);
		pDot11f->acvo_txoplimit =
			pe_session->gLimEdcaParamsBC[3].txoplimit;

		pDot11f->present = 1;
	}

} /* End PopluateDot11fEDCAParamSet. */

QDF_STATUS
populate_dot11f_erp_info(struct mac_context *mac,
			 tDot11fIEERPInfo *pDot11f, struct pe_session *pe_session)
{
	uint32_t val;
	enum reg_wifi_band rfBand = REG_BAND_UNKNOWN;

	lim_get_rf_band_new(mac, &rfBand, pe_session);
	if (REG_BAND_2G == rfBand) {
		pDot11f->present = 1;

		val = pe_session->cfgProtection.fromllb;
		if (!val) {
			pe_err("11B protection not enabled. Not populating ERP IE %d",
				val);
			return QDF_STATUS_SUCCESS;
		}

		if (pe_session->gLim11bParams.protectionEnabled) {
			pDot11f->non_erp_present = 1;
			pDot11f->use_prot = 1;
		}

		if (pe_session->gLimOlbcParams.protectionEnabled) {
			/* FIXME_PROTECTION: we should be setting non_erp present also. */
			/* check the test plan first. */
			pDot11f->use_prot = 1;
		}

		if ((pe_session->gLimNoShortParams.numNonShortPreambleSta)
		    || !pe_session->beaconParams.fShortPreamble) {
			pDot11f->barker_preamble = 1;

		}
	}

	return QDF_STATUS_SUCCESS;
} /* End populate_dot11f_erp_info. */

QDF_STATUS
populate_dot11f_ext_supp_rates(struct mac_context *mac, uint8_t nChannelNum,
			       tDot11fIEExtSuppRates *pDot11f,
			       struct pe_session *pe_session)
{
	QDF_STATUS nsir_status;
	qdf_size_t n_rates = 0;
	uint8_t rates[WLAN_SUPPORTED_RATES_IE_MAX_LEN];

	/* Use the ext rates present in session entry whenever nChannelNum is set to OPERATIONAL
	   else use the ext supported rate set from CFG, which is fixed and does not change dynamically and is used for
	   sending mgmt frames (lile probe req) which need to go out before any session is present.
	 */
	if (POPULATE_DOT11F_RATES_OPERATIONAL == nChannelNum) {
		if (pe_session) {
			n_rates = pe_session->extRateSet.numRates;
			qdf_mem_copy(rates, pe_session->extRateSet.rate,
				     n_rates);
		} else {
			pe_err("no session context exists while populating Operational Rate Set");
		}
	} else if (HIGHEST_24GHZ_CHANNEL_NUM >= nChannelNum) {
		if (!pe_session) {
			pe_err("null pe_session");
			return QDF_STATUS_E_INVAL;
		}
		n_rates = WLAN_SUPPORTED_RATES_IE_MAX_LEN;
		nsir_status = mlme_get_ext_opr_rate(pe_session->vdev, rates,
						    &n_rates);
		if (QDF_IS_STATUS_ERROR(nsir_status)) {
			n_rates = 0;
			pe_err("Failed to retrieve nItem from CFG status: %d",
			       (nsir_status));
			return nsir_status;
		}
	}

	if (0 != n_rates) {
		pe_debug("ext supp rates present, num %d", (uint8_t)n_rates);
		pDot11f->num_rates = (uint8_t)n_rates;
		qdf_mem_copy(pDot11f->rates, rates, n_rates);
		pDot11f->present = 1;
	}

	return QDF_STATUS_SUCCESS;

} /* End populate_dot11f_ext_supp_rates. */

QDF_STATUS
populate_dot11f_ext_supp_rates1(struct mac_context *mac,
				uint8_t nChannelNum,
				tDot11fIEExtSuppRates *pDot11f)
{
	qdf_size_t nRates;
	QDF_STATUS nsir_status;
	uint8_t rates[SIR_MAC_MAX_NUMBER_OF_RATES];

	if (14 < nChannelNum) {
		pDot11f->present = 0;
		return QDF_STATUS_SUCCESS;
	}
	/* N.B. I have *no* idea why we're calling 'wlan_cfg_get_str' with an argument */
	/* of WNI_CFG_SUPPORTED_RATES_11A here, but that's what was done */
	/* previously & I'm afraid to change it! */
	nRates = SIR_MAC_MAX_NUMBER_OF_RATES;
	nsir_status = wlan_mlme_get_cfg_str(
				rates,
				&mac->mlme_cfg->rates.supported_11a,
				&nRates);
	if (QDF_IS_STATUS_ERROR(nsir_status)) {
		pe_err("Failed to retrieve nItem from CFG status: %d",
		       (nsir_status));
		return nsir_status;
	}

	if (0 != nRates) {
		pDot11f->num_rates = (uint8_t) nRates;
		qdf_mem_copy(pDot11f->rates, rates, nRates);
		pDot11f->present = 1;
	}

	return QDF_STATUS_SUCCESS;
} /* populate_dot11f_ext_supp_rates1. */

QDF_STATUS
populate_dot11f_ht_caps(struct mac_context *mac,
			struct pe_session *pe_session, tDot11fIEHTCaps *pDot11f)
{
	qdf_size_t ncfglen;
	QDF_STATUS nSirStatus;
	uint8_t disable_high_ht_mcs_2x2 = 0;
	struct ch_params ch_params = {0};

	tSirMacTxBFCapabilityInfo *pTxBFCapabilityInfo;
	tSirMacASCapabilityInfo *pASCapabilityInfo;
	struct mlme_ht_capabilities_info *ht_cap_info;
	struct mlme_vht_capabilities_info *vht_cap_info;

	ht_cap_info = &mac->mlme_cfg->ht_caps.ht_cap_info;
	vht_cap_info = &mac->mlme_cfg->vht_caps.vht_cap_info;

	pDot11f->mimoPowerSave = ht_cap_info->mimo_power_save;
	pDot11f->greenField = ht_cap_info->green_field;
	pDot11f->delayedBA = ht_cap_info->delayed_ba;
	pDot11f->maximalAMSDUsize = ht_cap_info->maximal_amsdu_size;
	pDot11f->dsssCckMode40MHz = ht_cap_info->dsss_cck_mode_40_mhz;
	pDot11f->psmp = ht_cap_info->psmp;
	pDot11f->stbcControlFrame = ht_cap_info->stbc_control_frame;
	pDot11f->lsigTXOPProtection = ht_cap_info->l_sig_tx_op_protection;

	/* All sessionized entries will need the check below */
	if (!pe_session) {     /* Only in case of NO session */
		pDot11f->supportedChannelWidthSet =
			ht_cap_info->supported_channel_width_set;
		pDot11f->advCodingCap = ht_cap_info->adv_coding_cap;
		pDot11f->txSTBC = ht_cap_info->tx_stbc;
		pDot11f->rxSTBC = ht_cap_info->rx_stbc;
		pDot11f->shortGI20MHz = ht_cap_info->short_gi_20_mhz;
		pDot11f->shortGI40MHz = ht_cap_info->short_gi_40_mhz;
	} else {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(pe_session->curr_op_freq) &&
		    LIM_IS_STA_ROLE(pe_session) &&
		    WNI_CFG_CHANNEL_BONDING_MODE_DISABLE !=
		    mac->roam.configParam.channelBondingMode24GHz) {
			pDot11f->supportedChannelWidthSet = 1;
			ch_params.ch_width = CH_WIDTH_40MHZ;
			wlan_reg_set_channel_params_for_freq(
				mac->pdev, pe_session->curr_op_freq, 0,
				&ch_params);
			if (ch_params.ch_width != CH_WIDTH_40MHZ)
				pDot11f->supportedChannelWidthSet = 0;
			pe_debug("pe ch offset %d ch_width %d htcap ch width set %d",
				 pe_session->htSecondaryChannelOffset,
				 ch_params.ch_width,
				 pDot11f->supportedChannelWidthSet);
		} else {
			pDot11f->supportedChannelWidthSet =
				pe_session->htSupportedChannelWidthSet;
		}

		pDot11f->advCodingCap = pe_session->ht_config.ht_rx_ldpc;
		pDot11f->txSTBC = pe_session->ht_config.ht_tx_stbc;
		pDot11f->rxSTBC = pe_session->ht_config.ht_rx_stbc;
		pDot11f->shortGI20MHz = pe_session->ht_config.ht_sgi20;
		pDot11f->shortGI40MHz = pe_session->ht_config.ht_sgi40;
	}

	/* Ensure that shortGI40MHz is Disabled if supportedChannelWidthSet is
	   eHT_CHANNEL_WIDTH_20MHZ */
	if (pDot11f->supportedChannelWidthSet == eHT_CHANNEL_WIDTH_20MHZ) {
		pDot11f->shortGI40MHz = 0;
	}

	pDot11f->maxRxAMPDUFactor =
		mac->mlme_cfg->ht_caps.ampdu_params.max_rx_ampdu_factor;
	pDot11f->mpduDensity =
		mac->mlme_cfg->ht_caps.ampdu_params.mpdu_density;
	pDot11f->reserved1 = mac->mlme_cfg->ht_caps.ampdu_params.reserved;

	ncfglen = SIZE_OF_SUPPORTED_MCS_SET;
	nSirStatus = wlan_mlme_get_cfg_str(
		pDot11f->supportedMCSSet,
		&mac->mlme_cfg->rates.supported_mcs_set,
		&ncfglen);
	if (QDF_IS_STATUS_ERROR(nSirStatus)) {
		pe_err("Failed to retrieve nItem from CFG status: %d",
		       (nSirStatus));
			return nSirStatus;
	}

	if (pe_session) {
		disable_high_ht_mcs_2x2 =
				mac->mlme_cfg->rates.disable_high_ht_mcs_2x2;
		if (pe_session->nss == NSS_1x1_MODE) {
			pDot11f->supportedMCSSet[1] = 0;
			pDot11f->txSTBC = 0;
		} else if (wlan_reg_is_24ghz_ch_freq(
			   pe_session->curr_op_freq) &&
			   disable_high_ht_mcs_2x2 &&
			   (pe_session->opmode == QDF_STA_MODE)) {
			pe_debug("Disabling high HT MCS [%d]",
				 disable_high_ht_mcs_2x2);
			pDot11f->supportedMCSSet[1] =
					(pDot11f->supportedMCSSet[1] >>
						disable_high_ht_mcs_2x2);
		}
	}

	/* If STA mode, session supported NSS > 1 and
	 * SMPS enabled publish HT SMPS IE
	 */
	if (pe_session &&
	    LIM_IS_STA_ROLE(pe_session) &&
	    (pe_session->enableHtSmps) &&
	    (!pe_session->supported_nss_1x1)) {
		pe_debug("Add SM power save IE: %d",
			pe_session->htSmpsvalue);
		pDot11f->mimoPowerSave = pe_session->htSmpsvalue;
	}

	pDot11f->pco = mac->mlme_cfg->ht_caps.ext_cap_info.pco;
	pDot11f->transitionTime =
		mac->mlme_cfg->ht_caps.ext_cap_info.transition_time;
	pDot11f->mcsFeedback =
		mac->mlme_cfg->ht_caps.ext_cap_info.mcs_feedback;

	pTxBFCapabilityInfo =
		(tSirMacTxBFCapabilityInfo *)&vht_cap_info->tx_bf_cap;
	pDot11f->txBF = pTxBFCapabilityInfo->txBF;
	pDot11f->rxStaggeredSounding = pTxBFCapabilityInfo->rxStaggeredSounding;
	pDot11f->txStaggeredSounding = pTxBFCapabilityInfo->txStaggeredSounding;
	pDot11f->rxZLF = pTxBFCapabilityInfo->rxZLF;
	pDot11f->txZLF = pTxBFCapabilityInfo->txZLF;
	pDot11f->implicitTxBF = pTxBFCapabilityInfo->implicitTxBF;
	pDot11f->calibration = pTxBFCapabilityInfo->calibration;
	pDot11f->explicitCSITxBF = pTxBFCapabilityInfo->explicitCSITxBF;
	pDot11f->explicitUncompressedSteeringMatrix =
		pTxBFCapabilityInfo->explicitUncompressedSteeringMatrix;
	pDot11f->explicitBFCSIFeedback =
		pTxBFCapabilityInfo->explicitBFCSIFeedback;
	pDot11f->explicitUncompressedSteeringMatrixFeedback =
		pTxBFCapabilityInfo->explicitUncompressedSteeringMatrixFeedback;
	pDot11f->explicitCompressedSteeringMatrixFeedback =
		pTxBFCapabilityInfo->explicitCompressedSteeringMatrixFeedback;
	pDot11f->csiNumBFAntennae = pTxBFCapabilityInfo->csiNumBFAntennae;
	pDot11f->uncompressedSteeringMatrixBFAntennae =
		pTxBFCapabilityInfo->uncompressedSteeringMatrixBFAntennae;
	pDot11f->compressedSteeringMatrixBFAntennae =
		pTxBFCapabilityInfo->compressedSteeringMatrixBFAntennae;

	pASCapabilityInfo = (tSirMacASCapabilityInfo *)&vht_cap_info->as_cap;
	pDot11f->antennaSelection = pASCapabilityInfo->antennaSelection;
	pDot11f->explicitCSIFeedbackTx =
		pASCapabilityInfo->explicitCSIFeedbackTx;
	pDot11f->antennaIndicesFeedbackTx =
		pASCapabilityInfo->antennaIndicesFeedbackTx;
	pDot11f->explicitCSIFeedback = pASCapabilityInfo->explicitCSIFeedback;
	pDot11f->antennaIndicesFeedback =
		pASCapabilityInfo->antennaIndicesFeedback;
	pDot11f->rxAS = pASCapabilityInfo->rxAS;
	pDot11f->txSoundingPPDUs = pASCapabilityInfo->txSoundingPPDUs;

	pDot11f->present = 1;

	return QDF_STATUS_SUCCESS;

} /* End populate_dot11f_ht_caps. */

void lim_log_vht_cap(struct mac_context *mac, tDot11fIEVHTCaps *pDot11f)
{
#ifdef DUMP_MGMT_CNTNTS
	pe_debug("maxMPDULen (2): %d", pDot11f->maxMPDULen);
	pe_debug("supportedChannelWidthSet (2): %d",
		pDot11f->supportedChannelWidthSet);
	pe_debug("ldpcCodingCap (1): %d",
		pDot11f->ldpcCodingCap);
	pe_debug("shortGI80MHz (1): %d", pDot11f->shortGI80MHz);
	pe_debug("shortGI160and80plus80MHz (1): %d",
		pDot11f->shortGI160and80plus80MHz);
	pe_debug("txSTBC (1): %d", pDot11f->txSTBC);
	pe_debug("rxSTBC (3): %d", pDot11f->rxSTBC);
	pe_debug("suBeamFormerCap (1): %d",
		pDot11f->suBeamFormerCap);
	pe_debug("suBeamformeeCap (1): %d",
		pDot11f->suBeamformeeCap);
	pe_debug("csnofBeamformerAntSup (3): %d",
		pDot11f->csnofBeamformerAntSup);
	pe_debug("numSoundingDim (3): %d",
		pDot11f->numSoundingDim);
	pe_debug("muBeamformerCap (1): %d",
		pDot11f->muBeamformerCap);
	pe_debug("muBeamformeeCap (1): %d",
		pDot11f->muBeamformeeCap);
	pe_debug("vhtTXOPPS (1): %d", pDot11f->vhtTXOPPS);
	pe_debug("htcVHTCap (1): %d", pDot11f->htcVHTCap);
	pe_debug("maxAMPDULenExp (3): %d",
		pDot11f->maxAMPDULenExp);
	pe_debug("vhtLinkAdaptCap (2): %d",
		pDot11f->vhtLinkAdaptCap);
	pe_debug("rxAntPattern (1): %d",
		pDot11f->rxAntPattern;
	pe_debug("txAntPattern (1): %d",
		pDot11f->txAntPattern);
	pe_debug("reserved1 (2): %d", pDot11f->reserved1);
	pe_debug("rxMCSMap (16): %d", pDot11f->rxMCSMap);
	pe_debug("rxHighSupDataRate (13): %d",
		pDot11f->rxHighSupDataRate);
	pe_debug("reserved2(3): %d", pDot11f->reserved2);
	pe_debug("txMCSMap (16): %d", pDot11f->txMCSMap);
	pe_debug("txSupDataRate (13): %d"),
		pDot11f->txSupDataRate;
	pe_debug("reserved3 (3): %d", pDot11f->reserved3);
#endif /* DUMP_MGMT_CNTNTS */
}

static void lim_log_vht_operation(struct mac_context *mac,
				  tDot11fIEVHTOperation *pDot11f)
{
#ifdef DUMP_MGMT_CNTNTS
	pe_debug("chanWidth: %d", pDot11f->chanWidth);
	pe_debug("chan_center_freq_seg0: %d",
		 pDot11f->chan_center_freq_seg0);
	pe_debug("chan_center_freq_seg1: %d",
		 pDot11f->chan_center_freq_seg1);
	pe_debug("basicMCSSet: %d", pDot11f->basicMCSSet);
#endif /* DUMP_MGMT_CNTNTS */
}

static void lim_log_operating_mode(struct mac_context *mac,
				   tDot11fIEOperatingMode *pDot11f)
{
#ifdef DUMP_MGMT_CNTNTS
	pe_debug("ChanWidth: %d", pDot11f->chanWidth);
	pe_debug("reserved: %d", pDot11f->reserved);
	pe_debug("rxNSS: %d", pDot11f->rxNSS);
	pe_debug("rxNSS Type: %d", pDot11f->rxNSSType);
#endif /* DUMP_MGMT_CNTNTS */
}

static void lim_log_qos_map_set(struct mac_context *mac,
				struct qos_map_set *pQosMapSet)
{
	uint8_t i;

	if (pQosMapSet->num_dscp_exceptions > QOS_MAP_MAX_EX)
		pQosMapSet->num_dscp_exceptions = QOS_MAP_MAX_EX;

	pe_debug("num of dscp exceptions: %d",
		pQosMapSet->num_dscp_exceptions);
	for (i = 0; i < pQosMapSet->num_dscp_exceptions; i++)
		pe_nofl_debug("dscp value: %d, User priority value: %d",
			      pQosMapSet->dscp_exceptions[i][0],
			      pQosMapSet->dscp_exceptions[i][1]);

	for (i = 0; i < 8; i++)
		pe_nofl_debug("For up %d: dscp low: %d, dscp high: %d", i,
			       pQosMapSet->dscp_range[i][0],
			       pQosMapSet->dscp_range[i][1]);
}

QDF_STATUS
populate_dot11f_vht_caps(struct mac_context *mac,
			 struct pe_session *pe_session, tDot11fIEVHTCaps *pDot11f)
{
	uint32_t nCfgValue = 0;
	struct mlme_vht_capabilities_info *vht_cap_info;

	if (!(mac->mlme_cfg)) {
		pe_err("invalid mlme cfg");
		return QDF_STATUS_E_FAILURE;
	}
	vht_cap_info = &mac->mlme_cfg->vht_caps.vht_cap_info;

	pDot11f->present = 1;

	nCfgValue = vht_cap_info->ampdu_len;
	pDot11f->maxMPDULen = (nCfgValue & 0x0003);

	nCfgValue = vht_cap_info->supp_chan_width;
	pDot11f->supportedChannelWidthSet = (nCfgValue & 0x0003);

	nCfgValue = 0;
	/* With VHT it suffices if we just examine HT */
	if (pe_session) {
		if (lim_is_he_6ghz_band(pe_session)) {
			pDot11f->present = 0;
			return QDF_STATUS_SUCCESS;
		}

		if (pe_session->ht_config.ht_rx_ldpc)
			pDot11f->ldpcCodingCap =
				pe_session->vht_config.ldpc_coding;
		if (pe_session->ch_width < CH_WIDTH_80MHZ) {
			 pDot11f->shortGI80MHz = 0;
		} else {
			pDot11f->shortGI80MHz =
				pe_session->vht_config.shortgi80;
		}

		if (pe_session->ch_width < CH_WIDTH_160MHZ) {
			pDot11f->shortGI160and80plus80MHz = 0;
			pDot11f->supportedChannelWidthSet = 0;
		} else {
			pDot11f->shortGI160and80plus80MHz =
				pe_session->vht_config.shortgi160and80plus80;
		}

		if (pe_session->ht_config.ht_tx_stbc)
			pDot11f->txSTBC = pe_session->vht_config.tx_stbc;

		if (pe_session->ht_config.ht_rx_stbc)
			pDot11f->rxSTBC = pe_session->vht_config.rx_stbc;

		pDot11f->suBeamformeeCap =
			pe_session->vht_config.su_beam_formee;
		if (pe_session->vht_config.su_beam_formee) {
			pDot11f->muBeamformeeCap =
				pe_session->vht_config.mu_beam_formee;
			pDot11f->csnofBeamformerAntSup =
			      pe_session->vht_config.csnof_beamformer_antSup;
		} else {
			pDot11f->muBeamformeeCap = 0;
		}
		pDot11f->suBeamFormerCap =
			pe_session->vht_config.su_beam_former;

		pDot11f->vhtTXOPPS = pe_session->vht_config.vht_txops;

		pDot11f->numSoundingDim =
				pe_session->vht_config.num_soundingdim;

		pDot11f->htcVHTCap = pe_session->vht_config.htc_vhtcap;

		pDot11f->rxAntPattern = pe_session->vht_config.rx_antpattern;

		pDot11f->txAntPattern = pe_session->vht_config.tx_antpattern;
		pDot11f->extended_nss_bw_supp =
			pe_session->vht_config.extended_nss_bw_supp;

		pDot11f->maxAMPDULenExp =
				pe_session->vht_config.max_ampdu_lenexp;

		pDot11f->vhtLinkAdaptCap =
				pe_session->vht_config.vht_link_adapt;
	} else {
		nCfgValue = vht_cap_info->ldpc_coding_cap;
		pDot11f->ldpcCodingCap = (nCfgValue & 0x0001);

		nCfgValue = vht_cap_info->short_gi_80mhz;
		pDot11f->shortGI80MHz = (nCfgValue & 0x0001);

		nCfgValue = vht_cap_info->short_gi_160mhz;
		pDot11f->shortGI160and80plus80MHz = (nCfgValue & 0x0001);

		nCfgValue = vht_cap_info->tx_stbc;
		pDot11f->txSTBC = (nCfgValue & 0x0001);

		nCfgValue = vht_cap_info->rx_stbc;
		pDot11f->rxSTBC = (nCfgValue & 0x0007);

		nCfgValue = vht_cap_info->su_bformee;
		pDot11f->suBeamformeeCap = (nCfgValue & 0x0001);

		nCfgValue = vht_cap_info->enable_mu_bformee;
		pDot11f->muBeamformeeCap = (nCfgValue & 0x0001);

		nCfgValue = vht_cap_info->su_bformer;
		pDot11f->suBeamFormerCap = (nCfgValue & 0x0001);

		nCfgValue = vht_cap_info->tx_bfee_ant_supp;
		pDot11f->csnofBeamformerAntSup = (nCfgValue & 0x0007);

		nCfgValue = vht_cap_info->txop_ps;
		pDot11f->vhtTXOPPS = (nCfgValue & 0x0001);

		nCfgValue = vht_cap_info->num_soundingdim;
		pDot11f->numSoundingDim = (nCfgValue & 0x0007);

		nCfgValue = vht_cap_info->htc_vhtc;
		pDot11f->htcVHTCap = (nCfgValue & 0x0001);

		pDot11f->rxAntPattern = vht_cap_info->rx_antpattern;

		pDot11f->txAntPattern = vht_cap_info->tx_antpattern;

		nCfgValue = vht_cap_info->ampdu_len_exponent;
		pDot11f->maxAMPDULenExp = (nCfgValue & 0x0007);

		nCfgValue = vht_cap_info->link_adap_cap;
		pDot11f->vhtLinkAdaptCap = (nCfgValue & 0x0003);

		pDot11f->extended_nss_bw_supp =
			vht_cap_info->extended_nss_bw_supp;
	}

	pDot11f->max_nsts_total = vht_cap_info->max_nsts_total;
	pDot11f->vht_extended_nss_bw_cap =
		vht_cap_info->vht_extended_nss_bw_cap;

	nCfgValue = vht_cap_info->mu_bformer;
	pDot11f->muBeamformerCap = (nCfgValue & 0x0001);


	nCfgValue = vht_cap_info->rx_mcs_map;
	pDot11f->rxMCSMap = (nCfgValue & 0x0000FFFF);

	nCfgValue = vht_cap_info->rx_supp_data_rate;
	pDot11f->rxHighSupDataRate = (nCfgValue & 0x00001FFF);

	nCfgValue = vht_cap_info->tx_mcs_map;
	pDot11f->txMCSMap = (nCfgValue & 0x0000FFFF);

	nCfgValue = vht_cap_info->tx_supp_data_rate;
	pDot11f->txSupDataRate = (nCfgValue & 0x00001FFF);

	if (pe_session) {
		if (pe_session->nss == NSS_1x1_MODE) {
			pDot11f->txMCSMap |= DISABLE_NSS2_MCS;
			pDot11f->rxMCSMap |= DISABLE_NSS2_MCS;
			pDot11f->txSupDataRate =
				VHT_TX_HIGHEST_SUPPORTED_DATA_RATE_1_1;
			pDot11f->rxHighSupDataRate =
				VHT_RX_HIGHEST_SUPPORTED_DATA_RATE_1_1;
			if (!pe_session->ch_width &&
			    !vht_cap_info->enable_vht20_mcs9 &&
			    ((pDot11f->txMCSMap & VHT_1x1_MCS_MASK) ==
			     VHT_1x1_MCS9_MAP)) {
				DISABLE_VHT_MCS_9(pDot11f->txMCSMap,
						NSS_1x1_MODE);
				DISABLE_VHT_MCS_9(pDot11f->rxMCSMap,
						NSS_1x1_MODE);
			}
			pDot11f->txSTBC = 0;
		} else {
			if (!pe_session->ch_width &&
			    !vht_cap_info->enable_vht20_mcs9 &&
			    ((pDot11f->txMCSMap & VHT_2x2_MCS_MASK) ==
			     VHT_2x2_MCS9_MAP)) {
				DISABLE_VHT_MCS_9(pDot11f->txMCSMap,
						NSS_2x2_MODE);
				DISABLE_VHT_MCS_9(pDot11f->rxMCSMap,
						NSS_2x2_MODE);
			}
		}
	}

	lim_log_vht_cap(mac, pDot11f);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
populate_dot11f_vht_operation(struct mac_context *mac,
			      struct pe_session *pe_session,
			      tDot11fIEVHTOperation *pDot11f)
{
	uint32_t mcs_set;
	struct mlme_vht_capabilities_info *vht_cap_info;

	if (!pe_session || !pe_session->vhtCapability)
		return QDF_STATUS_SUCCESS;

	pDot11f->present = 1;

	if (pe_session->ch_width > CH_WIDTH_40MHZ) {
		pDot11f->chanWidth = 1;
		pDot11f->chan_center_freq_seg0 =
			pe_session->ch_center_freq_seg0;
		if (pe_session->ch_width == CH_WIDTH_80P80MHZ ||
				pe_session->ch_width == CH_WIDTH_160MHZ)
			pDot11f->chan_center_freq_seg1 =
				pe_session->ch_center_freq_seg1;
		else
			pDot11f->chan_center_freq_seg1 = 0;
	} else {
		pDot11f->chanWidth = 0;
		pDot11f->chan_center_freq_seg0 = 0;
		pDot11f->chan_center_freq_seg1 = 0;
	}

	vht_cap_info = &mac->mlme_cfg->vht_caps.vht_cap_info;
	mcs_set = vht_cap_info->basic_mcs_set;
	mcs_set = (mcs_set & 0xFFFC) | vht_cap_info->rx_mcs;

	if (pe_session->nss == NSS_1x1_MODE)
		mcs_set |= 0x000C;
	else
		mcs_set = (mcs_set & 0xFFF3) | (vht_cap_info->rx_mcs2x2 << 2);

	pDot11f->basicMCSSet = (uint16_t)mcs_set;
	lim_log_vht_operation(mac, pDot11f);

	return QDF_STATUS_SUCCESS;

}

QDF_STATUS
populate_dot11f_ext_cap(struct mac_context *mac,
			bool isVHTEnabled, tDot11fIEExtCap *pDot11f,
			struct pe_session *pe_session)
{
	struct s_ext_cap *p_ext_cap;

	pDot11f->present = 1;

	if (!pe_session) {
		pe_debug("11MC - enabled for non-SAP cases");
		pDot11f->num_bytes = DOT11F_IE_EXTCAP_MAX_LEN;
	} else if (pe_session->sap_dot11mc) {
		pe_debug("11MC support enabled");
		pDot11f->num_bytes = DOT11F_IE_EXTCAP_MAX_LEN;
	} else {
		if (eLIM_AP_ROLE != pe_session->limSystemRole)
			pDot11f->num_bytes = DOT11F_IE_EXTCAP_MAX_LEN;
		else
			pDot11f->num_bytes = DOT11F_IE_EXTCAP_MIN_LEN;
	}

	p_ext_cap = (struct s_ext_cap *)pDot11f->bytes;
	if (isVHTEnabled == true)
		p_ext_cap->oper_mode_notification = 1;

	if (mac->mlme_cfg->gen.rtt3_enabled) {
		uint32_t ftm = ucfg_wifi_pos_get_ftm_cap(mac->psoc);
		if (!pe_session || LIM_IS_STA_ROLE(pe_session)) {
			p_ext_cap->fine_time_meas_initiator =
				(ftm & WMI_FW_STA_RTT_INITR) ? 1 : 0;
			p_ext_cap->fine_time_meas_responder =
				(ftm & WMI_FW_STA_RTT_RESPR) ? 1 : 0;
		} else if (LIM_IS_AP_ROLE(pe_session)) {
			p_ext_cap->fine_time_meas_initiator =
				(ftm & WMI_FW_AP_RTT_INITR) ? 1 : 0;
			p_ext_cap->fine_time_meas_responder =
				(ftm & WMI_FW_AP_RTT_RESPR) ? 1 : 0;
		}
	}
#ifdef QCA_HT_2040_COEX
	if (mac->roam.configParam.obssEnabled)
		p_ext_cap->bss_coexist_mgmt_support = 1;
#endif
	p_ext_cap->ext_chan_switch = 1;

	if (pe_session && pe_session->enable_bcast_probe_rsp)
		p_ext_cap->fils_capability = 1;

	if (pe_session && pe_session->is_mbssid_enabled)
		p_ext_cap->multi_bssid = 1;

	p_ext_cap->beacon_protection_enable = pe_session ?
			mlme_get_bigtk_support(pe_session->vdev) : false;

	if (pe_session)
		populate_dot11f_twt_extended_caps(mac, pe_session, pDot11f);

	/* Need to calculate the num_bytes based on bits set */
	if (pDot11f->present)
		pDot11f->num_bytes = lim_compute_ext_cap_ie_length(pDot11f);

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11AX
static void populate_dot11f_qcn_ie_he_params(struct mac_context *mac,
					     struct pe_session *pe_session,
					     tDot11fIEqcn_ie *qcn_ie,
					     uint8_t attr_id)
{
	uint16_t mcs_12_13_supp;

	/* To fix WAPI IoT issue.*/
	if (pe_session->encryptType == eSIR_ED_WPI)
		return;
	if (wlan_reg_is_24ghz_ch_freq(pe_session->curr_op_freq))
		mcs_12_13_supp = mac->mlme_cfg->he_caps.he_mcs_12_13_supp_2g;
	else
		mcs_12_13_supp = mac->mlme_cfg->he_caps.he_mcs_12_13_supp_5g;

	if (!mcs_12_13_supp)
		return;

	qcn_ie->present = 1;
	qcn_ie->he_mcs13_attr.present = 1;
	qcn_ie->he_mcs13_attr.he_mcs_12_13_supp_80 = mcs_12_13_supp & 0xFF;
	qcn_ie->he_mcs13_attr.he_mcs_12_13_supp_160 = mcs_12_13_supp >> 8;
}
#else /* WLAN_FEATURE_11AX */
static void populate_dot11f_qcn_ie_he_params(struct mac_context *mac,
					     struct pe_session *pe_session,
					     tDot11fIEqcn_ie *qcn_ie,
					     uint8_t attr_id)
{}
#endif /* WLAN_FEATURE_11AX */

void populate_dot11f_qcn_ie(struct mac_context *mac,
			    struct pe_session *pe_session,
			    tDot11fIEqcn_ie *qcn_ie,
			    uint8_t attr_id)
{
	qcn_ie->present = 0;
	if (mac->mlme_cfg->sta.qcn_ie_support &&
	    ((attr_id == QCN_IE_ATTR_ID_ALL) ||
	    (attr_id == QCN_IE_ATTR_ID_VERSION))) {
		qcn_ie->present = 1;
		qcn_ie->qcn_version.present = 1;
		qcn_ie->qcn_version.version = QCN_IE_VERSION_SUPPORTED;
		qcn_ie->qcn_version.sub_version = QCN_IE_SUBVERSION_SUPPORTED;
	}
	if (mac->mlme_cfg->vht_caps.vht_cap_info.vht_mcs_10_11_supp) {
		qcn_ie->present = 1;
		qcn_ie->vht_mcs11_attr.present = 1;
		qcn_ie->vht_mcs11_attr.vht_mcs_10_11_supp = 1;
	}

	populate_dot11f_qcn_ie_he_params(mac, pe_session, qcn_ie, attr_id);
}

QDF_STATUS
populate_dot11f_operating_mode(struct mac_context *mac,
			       tDot11fIEOperatingMode *pDot11f,
			       struct pe_session *pe_session)
{
	pDot11f->present = 1;

	pDot11f->chanWidth = pe_session->gLimOperatingMode.chanWidth;
	pDot11f->rxNSS = pe_session->gLimOperatingMode.rxNSS;
	pDot11f->rxNSSType = pe_session->gLimOperatingMode.rxNSSType;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
populate_dot11f_ht_info(struct mac_context *mac,
			tDot11fIEHTInfo *pDot11f, struct pe_session *pe_session)
{
	qdf_size_t ncfglen;
	QDF_STATUS nSirStatus;

	if (!pe_session) {
		pe_err("Invalid session entry");
		return QDF_STATUS_E_FAILURE;
	}

	pDot11f->primaryChannel = wlan_reg_freq_to_chan(
		mac->pdev, pe_session->curr_op_freq);

	pDot11f->secondaryChannelOffset =
		pe_session->htSecondaryChannelOffset;
	pDot11f->recommendedTxWidthSet =
		pe_session->htRecommendedTxWidthSet;
	pDot11f->rifsMode = pe_session->beaconParams.fRIFSMode;
	pDot11f->controlledAccessOnly =
		mac->mlme_cfg->ht_caps.info_field_1.controlled_access_only;
	pDot11f->serviceIntervalGranularity =
		mac->lim.gHTServiceIntervalGranularity;

	if (LIM_IS_AP_ROLE(pe_session)) {
		pDot11f->opMode = pe_session->htOperMode;
		pDot11f->nonGFDevicesPresent =
			pe_session->beaconParams.llnNonGFCoexist;
		pDot11f->obssNonHTStaPresent =
			pe_session->beaconParams.gHTObssMode;
		pDot11f->reserved = 0;
	} else {
		pDot11f->opMode = 0;
		pDot11f->nonGFDevicesPresent = 0;
		pDot11f->obssNonHTStaPresent = 0;
		pDot11f->reserved = 0;
	}

	pDot11f->basicSTBCMCS = mac->lim.gHTSTBCBasicMCS;
	pDot11f->dualCTSProtection = mac->lim.gHTDualCTSProtection;
	pDot11f->secondaryBeacon = mac->lim.gHTSecondaryBeacon;
	pDot11f->lsigTXOPProtectionFullSupport =
		pe_session->beaconParams.fLsigTXOPProtectionFullSupport;
	pDot11f->pcoActive = mac->lim.gHTPCOActive;
	pDot11f->pcoPhase = mac->lim.gHTPCOPhase;
	pDot11f->reserved2 = 0;

	ncfglen = SIZE_OF_BASIC_MCS_SET;
	nSirStatus = wlan_mlme_get_cfg_str(pDot11f->basicMCSSet,
					   &mac->mlme_cfg->rates.basic_mcs_set,
					   &ncfglen);
	if (QDF_IS_STATUS_ERROR(nSirStatus)) {
		pe_err("Failed to retrieve nItem from CFG status: %d",
		       (nSirStatus));
		return nSirStatus;
	}

	pDot11f->present = 1;

	return QDF_STATUS_SUCCESS;

} /* End populate_dot11f_ht_info. */

#ifdef ANI_SUPPORT_11H
QDF_STATUS
populate_dot11f_measurement_report0(struct mac_context *mac,
				    tpSirMacMeasReqActionFrame pReq,
				    tDot11fIEMeasurementReport *pDot11f)
{
	pDot11f->token = pReq->measReqIE.measToken;
	pDot11f->late = 0;
	pDot11f->incapable = 0;
	pDot11f->refused = 1;
	pDot11f->type = SIR_MAC_BASIC_MEASUREMENT_TYPE;

	pDot11f->present = 1;

	return QDF_STATUS_SUCCESS;

} /* End PopulatedDot11fMeasurementReport0. */
QDF_STATUS
populate_dot11f_measurement_report1(struct mac_context *mac,
				    tpSirMacMeasReqActionFrame pReq,
				    tDot11fIEMeasurementReport *pDot11f)
{
	pDot11f->token = pReq->measReqIE.measToken;
	pDot11f->late = 0;
	pDot11f->incapable = 0;
	pDot11f->refused = 1;
	pDot11f->type = SIR_MAC_CCA_MEASUREMENT_TYPE;
	pDot11f->present = 1;
	return QDF_STATUS_SUCCESS;
} /* End PopulatedDot11fMeasurementReport1. */
QDF_STATUS
populate_dot11f_measurement_report2(struct mac_context *mac,
				    tpSirMacMeasReqActionFrame pReq,
				    tDot11fIEMeasurementReport *pDot11f)
{
	pDot11f->token = pReq->measReqIE.measToken;
	pDot11f->late = 0;
	pDot11f->incapable = 0;
	pDot11f->refused = 1;
	pDot11f->type = SIR_MAC_RPI_MEASUREMENT_TYPE;
	pDot11f->present = 1;
	return QDF_STATUS_SUCCESS;
} /* End PopulatedDot11fMeasurementReport2. */
#endif

void
populate_dot11f_power_caps(struct mac_context *mac,
			   tDot11fIEPowerCaps *pCaps,
			   uint8_t nAssocType, struct pe_session *pe_session)
{
	struct vdev_mlme_obj *mlme_obj;

	if (nAssocType == LIM_REASSOC) {
		pCaps->minTxPower =
			pe_session->pLimReAssocReq->powerCap.minTxPower;
		pCaps->maxTxPower =
			pe_session->pLimReAssocReq->powerCap.maxTxPower;
	} else {
		pCaps->minTxPower =
			pe_session->lim_join_req->powerCap.minTxPower;
		pCaps->maxTxPower =
			pe_session->lim_join_req->powerCap.maxTxPower;

	}

	/* Use firmware updated max tx power if non zero */
	mlme_obj = wlan_vdev_mlme_get_cmpt_obj(pe_session->vdev);
	if (mlme_obj && mlme_obj->mgmt.generic.tx_pwrlimit)
		pCaps->maxTxPower = mlme_obj->mgmt.generic.tx_pwrlimit;

	pCaps->present = 1;
} /* End populate_dot11f_power_caps. */

QDF_STATUS
populate_dot11f_power_constraints(struct mac_context *mac,
				  tDot11fIEPowerConstraints *pDot11f)
{
	pDot11f->localPowerConstraints =
			mac->mlme_cfg->power.local_power_constraint;

	pDot11f->present = 1;

	return QDF_STATUS_SUCCESS;
} /* End populate_dot11f_power_constraints. */

void
populate_dot11f_qos_caps_station(struct mac_context *mac, struct pe_session *pe_session,
				 tDot11fIEQOSCapsStation *pDot11f)
{
	uint8_t max_sp_length = 0;

	max_sp_length = mac->mlme_cfg->wmm_params.max_sp_length;

	pDot11f->more_data_ack = 0;
	pDot11f->max_sp_length = max_sp_length;
	pDot11f->qack = 0;

	if (mac->lim.gUapsdEnable) {
		pDot11f->acbe_uapsd =
			LIM_UAPSD_GET(ACBE, pe_session->gUapsdPerAcBitmask);
		pDot11f->acbk_uapsd =
			LIM_UAPSD_GET(ACBK, pe_session->gUapsdPerAcBitmask);
		pDot11f->acvi_uapsd =
			LIM_UAPSD_GET(ACVI, pe_session->gUapsdPerAcBitmask);
		pDot11f->acvo_uapsd =
			LIM_UAPSD_GET(ACVO, pe_session->gUapsdPerAcBitmask);
	}
	pDot11f->present = 1;
} /* End PopulatedDot11fQOSCaps. */

QDF_STATUS
populate_dot11f_rsn(struct mac_context *mac,
		    tpSirRSNie pRsnIe, tDot11fIERSN *pDot11f)
{
	uint32_t status;
	int idx;

	if (pRsnIe->length) {
		idx = find_ie_location(mac, pRsnIe, DOT11F_EID_RSN);
		if (0 <= idx) {
			status = dot11f_unpack_ie_rsn(mac, pRsnIe->rsnIEdata + idx + 2,   /* EID, length */
						      pRsnIe->rsnIEdata[idx + 1],
						      pDot11f, false);
			if (DOT11F_FAILED(status)) {
				pe_err("Parse failure in Populate Dot11fRSN (0x%08x)",
					status);
				return QDF_STATUS_E_FAILURE;
			}
			pe_debug("status 0x%08x", status);
		}

	}

	return QDF_STATUS_SUCCESS;
} /* End populate_dot11f_rsn. */

QDF_STATUS populate_dot11f_rsn_opaque(struct mac_context *mac,
					 tpSirRSNie pRsnIe,
					 tDot11fIERSNOpaque *pDot11f)
{
	int idx;

	if (pRsnIe->length) {
		idx = find_ie_location(mac, pRsnIe, DOT11F_EID_RSN);
		if (0 <= idx) {
			pDot11f->present = 1;
			pDot11f->num_data = pRsnIe->rsnIEdata[idx + 1];
			qdf_mem_copy(pDot11f->data, pRsnIe->rsnIEdata + idx + 2,        /* EID, len */
				     pRsnIe->rsnIEdata[idx + 1]);
		}
	}

	return QDF_STATUS_SUCCESS;

} /* End populate_dot11f_rsn_opaque. */

#if defined(FEATURE_WLAN_WAPI)

QDF_STATUS
populate_dot11f_wapi(struct mac_context *mac,
		     tpSirRSNie pRsnIe, tDot11fIEWAPI *pDot11f)
{
	uint32_t status;
	int idx;

	if (pRsnIe->length) {
		idx = find_ie_location(mac, pRsnIe, DOT11F_EID_WAPI);
		if (0 <= idx) {
			status = dot11f_unpack_ie_wapi(mac, pRsnIe->rsnIEdata + idx + 2,  /* EID, length */
						       pRsnIe->rsnIEdata[idx + 1],
						       pDot11f, false);
			if (DOT11F_FAILED(status)) {
				pe_err("Parse failure (0x%08x)", status);
				return QDF_STATUS_E_FAILURE;
			}
			pe_debug("status 0x%08x", status);
		}
	}

	return QDF_STATUS_SUCCESS;
} /* End populate_dot11f_wapi. */

QDF_STATUS populate_dot11f_wapi_opaque(struct mac_context *mac,
					  tpSirRSNie pRsnIe,
					  tDot11fIEWAPIOpaque *pDot11f)
{
	int idx;

	if (pRsnIe->length) {
		idx = find_ie_location(mac, pRsnIe, DOT11F_EID_WAPI);
		if (0 <= idx) {
			pDot11f->present = 1;
			pDot11f->num_data = pRsnIe->rsnIEdata[idx + 1];
			qdf_mem_copy(pDot11f->data, pRsnIe->rsnIEdata + idx + 2,        /* EID, len */
				     pRsnIe->rsnIEdata[idx + 1]);
		}
	}

	return QDF_STATUS_SUCCESS;

} /* End populate_dot11f_wapi_opaque. */

#endif /* defined(FEATURE_WLAN_WAPI) */

void
populate_dot11f_ssid(struct mac_context *mac,
		     tSirMacSSid *pInternal, tDot11fIESSID *pDot11f)
{
	pDot11f->present = 1;
	pDot11f->num_ssid = pInternal->length;
	if (pInternal->length) {
		qdf_mem_copy((uint8_t *) pDot11f->ssid,
			     (uint8_t *) &pInternal->ssId, pInternal->length);
	}
} /* End populate_dot11f_ssid. */

QDF_STATUS populate_dot11f_ssid2(struct mac_context *mac, tDot11fIESSID *pDot11f)
{

	qdf_mem_copy(pDot11f->ssid, mac->mlme_cfg->sap_cfg.cfg_ssid,
		     mac->mlme_cfg->sap_cfg.cfg_ssid_len);
	pDot11f->num_ssid = mac->mlme_cfg->sap_cfg.cfg_ssid_len;
	pDot11f->present = 1;
	return QDF_STATUS_SUCCESS;
} /* End populate_dot11f_ssid2. */

void
populate_dot11f_schedule(tSirMacScheduleIE *pSchedule,
			 tDot11fIESchedule *pDot11f)
{
	pDot11f->aggregation = pSchedule->info.aggregation;
	pDot11f->tsid = pSchedule->info.tsid;
	pDot11f->direction = pSchedule->info.direction;
	pDot11f->reserved = pSchedule->info.rsvd;
	pDot11f->service_start_time = pSchedule->svcStartTime;
	pDot11f->service_interval = pSchedule->svcInterval;
	pDot11f->max_service_dur = pSchedule->maxSvcDuration;
	pDot11f->spec_interval = pSchedule->specInterval;

	pDot11f->present = 1;
} /* End populate_dot11f_schedule. */

void
populate_dot11f_supp_channels(struct mac_context *mac,
			      tDot11fIESuppChannels *pDot11f,
			      uint8_t nAssocType, struct pe_session *pe_session)
{
	uint8_t i;
	uint8_t *p;

	if (nAssocType == LIM_REASSOC) {
		p = (uint8_t *) pe_session->pLimReAssocReq->
		    supportedChannels.channelList;
		pDot11f->num_bands =
			pe_session->pLimReAssocReq->supportedChannels.numChnl;
	} else {
		p = (uint8_t *)pe_session->lim_join_req->supportedChannels.
		    channelList;
		pDot11f->num_bands =
			pe_session->lim_join_req->supportedChannels.numChnl;
	}
	for (i = 0U; i < pDot11f->num_bands; ++i, ++p) {
		pDot11f->bands[i][0] = *p;
		pDot11f->bands[i][1] = 1;
	}

	pDot11f->present = 1;

} /* End populate_dot11f_supp_channels. */

QDF_STATUS
populate_dot11f_supp_rates(struct mac_context *mac,
			   uint8_t nChannelNum,
			   tDot11fIESuppRates *pDot11f,
			   struct pe_session *pe_session)
{
	QDF_STATUS nsir_status;
	qdf_size_t nRates;
	uint8_t rates[SIR_MAC_MAX_NUMBER_OF_RATES];

	/* Use the operational rates present in session entry whenever
	 * nChannelNum is set to OPERATIONAL else use the supported
	 * rate set from CFG, which is fixed and does not change
	 * dynamically and is used for sending mgmt frames (lile probe
	 * req) which need to go out before any session is present.
	 */
	if (POPULATE_DOT11F_RATES_OPERATIONAL == nChannelNum) {
		if (pe_session) {
			nRates = pe_session->rateSet.numRates;
			qdf_mem_copy(rates, pe_session->rateSet.rate,
				     nRates);
		} else {
			pe_err("no session context exists while populating Operational Rate Set");
			nRates = 0;
		}
	} else if (14 >= nChannelNum) {
		nRates = SIR_MAC_MAX_NUMBER_OF_RATES;
		nsir_status = wlan_mlme_get_cfg_str(
					rates,
					&mac->mlme_cfg->rates.supported_11b,
					&nRates);
		if (QDF_IS_STATUS_ERROR(nsir_status)) {
			pe_err("Failed to retrieve nItem from CFG status: %d",
			       (nsir_status));
			return nsir_status;
		}
	} else {
		nRates = SIR_MAC_MAX_NUMBER_OF_RATES;
		nsir_status = wlan_mlme_get_cfg_str(
					rates,
					&mac->mlme_cfg->rates.supported_11a,
					&nRates);
		if (QDF_IS_STATUS_ERROR(nsir_status)) {
			pe_err("Failed to retrieve nItem from CFG status: %d",
			       (nsir_status));
			return nsir_status;
		}
	}

	if (0 != nRates) {
		pDot11f->num_rates = (uint8_t) nRates;
		qdf_mem_copy(pDot11f->rates, rates, nRates);
		pDot11f->present = 1;
	}

	return QDF_STATUS_SUCCESS;

} /* End populate_dot11f_supp_rates. */

/**
 * populate_dot11f_rates_tdls() - populate supported rates and
 *                                extended supported rates IE.
 * @p_mac gloabl - header.
 * @p_supp_rates - pointer to supported rates IE
 * @p_ext_supp_rates - pointer to extended supported rates IE
 * @curr_oper_channel - current operating channel
 *
 * This function populates the supported rates and extended supported
 * rates IE based in the STA capability. If the number of rates
 * supported is less than MAX_NUM_SUPPORTED_RATES, only supported rates
 * IE is populated.
 *
 * Return: QDF_STATUS QDF_STATUS_SUCCESS on Success and QDF_STATUS_E_FAILURE
 *         on failure.
 */

QDF_STATUS
populate_dot11f_rates_tdls(struct mac_context *p_mac,
			   tDot11fIESuppRates *p_supp_rates,
			   tDot11fIEExtSuppRates *p_ext_supp_rates,
			   uint8_t curr_oper_channel)
{
	tSirMacRateSet temp_rateset;
	tSirMacRateSet temp_rateset2;
	uint32_t i;
	uint32_t self_dot11mode = 0;
	qdf_size_t num_rates;

	self_dot11mode = p_mac->mlme_cfg->dot11_mode.dot11_mode;

	/**
	 * Include 11b rates only when the device configured in
	 * auto, 11a/b/g or 11b_only and also if current base
	 * channel is 5 GHz then no need to advertise the 11b rates.
	 * If devices move to 2.4GHz off-channel then they can communicate
	 * in 11g rates i.e. (6, 9, 12, 18, 24, 36 and 54).
	 */
	pe_debug("Current operating channel %d self_dot11mode = %d",
		curr_oper_channel, self_dot11mode);

	if ((curr_oper_channel <= SIR_11B_CHANNEL_END) &&
	    ((self_dot11mode == MLME_DOT11_MODE_ALL) ||
	    (self_dot11mode == MLME_DOT11_MODE_11A) ||
	    (self_dot11mode == MLME_DOT11_MODE_11AC) ||
	    (self_dot11mode == MLME_DOT11_MODE_11N) ||
	    (self_dot11mode == MLME_DOT11_MODE_11G) ||
	    (self_dot11mode == MLME_DOT11_MODE_11B))) {
		num_rates = p_mac->mlme_cfg->rates.supported_11b.len;
		wlan_mlme_get_cfg_str((uint8_t *)&temp_rateset.rate,
				      &p_mac->mlme_cfg->rates.supported_11b,
				      &num_rates);
		temp_rateset.numRates = (uint8_t)num_rates;
	} else {
	    temp_rateset.numRates = 0;
	}

	/* Include 11a rates when the device configured in non-11b mode */
	if (!IS_DOT11_MODE_11B(self_dot11mode)) {
		num_rates = p_mac->mlme_cfg->rates.supported_11a.len;
		wlan_mlme_get_cfg_str((uint8_t *)&temp_rateset2.rate,
				      &p_mac->mlme_cfg->rates.supported_11a,
				      &num_rates);
		temp_rateset2.numRates = (uint8_t)num_rates;
	} else {
		temp_rateset2.numRates = 0;
	}

	if ((temp_rateset.numRates + temp_rateset2.numRates) >
					SIR_MAC_MAX_NUMBER_OF_RATES) {
		pe_err("more than %d rates in CFG",
				SIR_MAC_MAX_NUMBER_OF_RATES);
		return QDF_STATUS_E_FAILURE;
	}

	/**
	 * copy all rates in temp_rateset,
	 * there are SIR_MAC_MAX_NUMBER_OF_RATES rates max
	 */
	for (i = 0; i < temp_rateset2.numRates; i++)
		temp_rateset.rate[i + temp_rateset.numRates] =
						temp_rateset2.rate[i];

	temp_rateset.numRates += temp_rateset2.numRates;

	if (temp_rateset.numRates <= MAX_NUM_SUPPORTED_RATES) {
		p_supp_rates->num_rates = temp_rateset.numRates;
		qdf_mem_copy(p_supp_rates->rates, temp_rateset.rate,
			     p_supp_rates->num_rates);
		p_supp_rates->present = 1;
	}  else { /* Populate extended capability as well */
		p_supp_rates->num_rates = MAX_NUM_SUPPORTED_RATES;
		qdf_mem_copy(p_supp_rates->rates, temp_rateset.rate,
			     p_supp_rates->num_rates);
		p_supp_rates->present = 1;

		p_ext_supp_rates->num_rates = temp_rateset.numRates -
				     MAX_NUM_SUPPORTED_RATES;
		qdf_mem_copy(p_ext_supp_rates->rates,
			     (uint8_t *)temp_rateset.rate +
			     MAX_NUM_SUPPORTED_RATES,
			     p_ext_supp_rates->num_rates);
		p_ext_supp_rates->present = 1;
	}

	return QDF_STATUS_SUCCESS;

} /* End populate_dot11f_rates_tdls */


QDF_STATUS
populate_dot11f_tpc_report(struct mac_context *mac,
			   tDot11fIETPCReport *pDot11f, struct pe_session *pe_session)
{
	uint16_t staid;
	uint8_t tx_power;
	QDF_STATUS nSirStatus;

	nSirStatus = lim_get_mgmt_staid(mac, &staid, pe_session);
	if (QDF_STATUS_SUCCESS != nSirStatus) {
		pe_err("Failed to get the STAID in Populate Dot11fTPCReport; lim_get_mgmt_staid returned status %d",
			nSirStatus);
		return QDF_STATUS_E_FAILURE;
	}
	tx_power = wlan_reg_get_channel_reg_power_for_freq(
				mac->pdev, pe_session->curr_op_freq);
	pDot11f->tx_power = tx_power;
	pDot11f->link_margin = 0;
	pDot11f->present = 1;

	return QDF_STATUS_SUCCESS;
} /* End populate_dot11f_tpc_report. */

void populate_dot11f_ts_info(struct mac_ts_info *pInfo,
			     tDot11fFfTSInfo *pDot11f)
{
	pDot11f->traffic_type = pInfo->traffic.trafficType;
	pDot11f->tsid = pInfo->traffic.tsid;
	pDot11f->direction = pInfo->traffic.direction;
	pDot11f->access_policy = pInfo->traffic.accessPolicy;
	pDot11f->aggregation = pInfo->traffic.aggregation;
	pDot11f->psb = pInfo->traffic.psb;
	pDot11f->user_priority = pInfo->traffic.userPrio;
	pDot11f->tsinfo_ack_pol = pInfo->traffic.ackPolicy;
	pDot11f->schedule = pInfo->schedule.schedule;
} /* End PopulatedDot11fTSInfo. */

void populate_dot11f_wmm(struct mac_context *mac,
			 tDot11fIEWMMInfoAp *pInfo,
			 tDot11fIEWMMParams *pParams,
			 tDot11fIEWMMCaps *pCaps, struct pe_session *pe_session)
{
	if (pe_session->limWmeEnabled) {
		populate_dot11f_wmm_params(mac, pParams,
					   pe_session);
		if (pe_session->limWsmEnabled)
			populate_dot11f_wmm_caps(pCaps);
	}
} /* End populate_dot11f_wmm. */

void populate_dot11f_wmm_caps(tDot11fIEWMMCaps *pCaps)
{
	pCaps->version = SIR_MAC_OUI_VERSION_1;
	pCaps->qack = 0;
	pCaps->queue_request = 1;
	pCaps->txop_request = 0;
	pCaps->more_ack = 0;
	pCaps->present = 1;
} /* End PopulateDot11fWmmCaps. */

#ifdef FEATURE_WLAN_ESE
void populate_dot11f_re_assoc_tspec(struct mac_context *mac,
				    tDot11fReAssocRequest *pReassoc,
				    struct pe_session *pe_session)
{
	uint8_t numTspecs = 0, idx;
	tTspecInfo *pTspec = NULL;

	numTspecs = pe_session->pLimReAssocReq->eseTspecInfo.numTspecs;
	pTspec = &pe_session->pLimReAssocReq->eseTspecInfo.tspec[0];
	pReassoc->num_WMMTSPEC = numTspecs;
	if (numTspecs) {
		for (idx = 0; idx < numTspecs; idx++) {
			populate_dot11f_wmmtspec(&pTspec->tspec,
						 &pReassoc->WMMTSPEC[idx]);
			pTspec->tspec.mediumTime = 0;
			pTspec++;
		}
	}
}

void ese_populate_wmm_tspec(struct mac_tspec_ie *source,
			    ese_wmm_tspec_ie *dest)
{
	dest->traffic_type = source->tsinfo.traffic.trafficType;
	dest->tsid = source->tsinfo.traffic.tsid;
	dest->direction = source->tsinfo.traffic.direction;
	dest->access_policy = source->tsinfo.traffic.accessPolicy;
	dest->aggregation = source->tsinfo.traffic.aggregation;
	dest->psb = source->tsinfo.traffic.psb;
	dest->user_priority = source->tsinfo.traffic.userPrio;
	dest->tsinfo_ack_pol = source->tsinfo.traffic.ackPolicy;
	dest->burst_size_defn = source->tsinfo.traffic.burstSizeDefn;
	/* As defined in IEEE 802.11-2007, section 7.3.2.30
	 * Nominal MSDU size: Bit[0:14]=Size, Bit[15]=Fixed
	 */
	dest->size = (source->nomMsduSz & SIZE_MASK);
	dest->fixed = (source->nomMsduSz & FIXED_MASK) ? 1 : 0;
	dest->max_msdu_size = source->maxMsduSz;
	dest->min_service_int = source->minSvcInterval;
	dest->max_service_int = source->maxSvcInterval;
	dest->inactivity_int = source->inactInterval;
	dest->suspension_int = source->suspendInterval;
	dest->service_start_time = source->svcStartTime;
	dest->min_data_rate = source->minDataRate;
	dest->mean_data_rate = source->meanDataRate;
	dest->peak_data_rate = source->peakDataRate;
	dest->burst_size = source->maxBurstSz;
	dest->delay_bound = source->delayBound;
	dest->min_phy_rate = source->minPhyRate;
	dest->surplus_bw_allowance = source->surplusBw;
	dest->medium_time = source->mediumTime;
}

#endif

void populate_dot11f_wmm_info_ap(struct mac_context *mac, tDot11fIEWMMInfoAp *pInfo,
				 struct pe_session *pe_session)
{
	pInfo->version = SIR_MAC_OUI_VERSION_1;

	/* WMM Specification 3.1.3, 3.2.3 */
	pInfo->param_set_count = (0xf & pe_session->gLimEdcaParamSetCount);
	if (LIM_IS_AP_ROLE(pe_session))
		pInfo->uapsd = (0x1 & pe_session->apUapsdEnable);
	else
		pInfo->uapsd = (0x1 & mac->lim.gUapsdEnable);

	pInfo->present = 1;
}

void populate_dot11f_wmm_info_station_per_session(struct mac_context *mac,
						  struct pe_session *pe_session,
						  tDot11fIEWMMInfoStation *pInfo)
{
	uint8_t max_sp_length = 0;

	max_sp_length = mac->mlme_cfg->wmm_params.max_sp_length;
	pInfo->version = SIR_MAC_OUI_VERSION_1;
	pInfo->acvo_uapsd =
		LIM_UAPSD_GET(ACVO, pe_session->gUapsdPerAcBitmask);
	pInfo->acvi_uapsd =
		LIM_UAPSD_GET(ACVI, pe_session->gUapsdPerAcBitmask);
	pInfo->acbk_uapsd =
		LIM_UAPSD_GET(ACBK, pe_session->gUapsdPerAcBitmask);
	pInfo->acbe_uapsd =
		LIM_UAPSD_GET(ACBE, pe_session->gUapsdPerAcBitmask);

	pInfo->max_sp_length = max_sp_length;
	pInfo->present = 1;
}

void populate_dot11f_wmm_params(struct mac_context *mac,
				tDot11fIEWMMParams *pParams,
				struct pe_session *pe_session)
{
	pParams->version = SIR_MAC_OUI_VERSION_1;

	if (LIM_IS_AP_ROLE(pe_session))
		pParams->qosInfo =
			(pe_session->
			 apUapsdEnable << 7) | ((uint8_t) (0x0f & pe_session->
							   gLimEdcaParamSetCount));
	else
		pParams->qosInfo =
			(mac->lim.
			 gUapsdEnable << 7) | ((uint8_t) (0x0f & pe_session->
							  gLimEdcaParamSetCount));

	/* Fill each EDCA parameter set in order: be, bk, vi, vo */
	pParams->acbe_aifsn =
		(0xf & SET_AIFSN(pe_session->gLimEdcaParamsBC[0].aci.aifsn));
	pParams->acbe_acm = (0x1 & pe_session->gLimEdcaParamsBC[0].aci.acm);
	pParams->acbe_aci = (0x3 & QCA_WLAN_AC_BE);
	pParams->acbe_acwmin =
		(0xf & pe_session->gLimEdcaParamsBC[0].cw.min);
	pParams->acbe_acwmax =
		(0xf & pe_session->gLimEdcaParamsBC[0].cw.max);
	pParams->acbe_txoplimit = pe_session->gLimEdcaParamsBC[0].txoplimit;

	pParams->acbk_aifsn =
		(0xf & SET_AIFSN(pe_session->gLimEdcaParamsBC[1].aci.aifsn));
	pParams->acbk_acm = (0x1 & pe_session->gLimEdcaParamsBC[1].aci.acm);
	pParams->acbk_aci = (0x3 & QCA_WLAN_AC_BK);
	pParams->acbk_acwmin =
		(0xf & pe_session->gLimEdcaParamsBC[1].cw.min);
	pParams->acbk_acwmax =
		(0xf & pe_session->gLimEdcaParamsBC[1].cw.max);
	pParams->acbk_txoplimit = pe_session->gLimEdcaParamsBC[1].txoplimit;

	if (LIM_IS_AP_ROLE(pe_session))
		pParams->acvi_aifsn =
			(0xf & pe_session->gLimEdcaParamsBC[2].aci.aifsn);
	else
		pParams->acvi_aifsn =
			(0xf &
			 SET_AIFSN(pe_session->gLimEdcaParamsBC[2].aci.aifsn));

	pParams->acvi_acm = (0x1 & pe_session->gLimEdcaParamsBC[2].aci.acm);
	pParams->acvi_aci = (0x3 & QCA_WLAN_AC_VI);
	pParams->acvi_acwmin =
		(0xf & pe_session->gLimEdcaParamsBC[2].cw.min);
	pParams->acvi_acwmax =
		(0xf & pe_session->gLimEdcaParamsBC[2].cw.max);
	pParams->acvi_txoplimit = pe_session->gLimEdcaParamsBC[2].txoplimit;

	if (LIM_IS_AP_ROLE(pe_session))
		pParams->acvo_aifsn =
			(0xf & pe_session->gLimEdcaParamsBC[3].aci.aifsn);
	else
		pParams->acvo_aifsn =
			(0xf &
			 SET_AIFSN(pe_session->gLimEdcaParamsBC[3].aci.aifsn));

	pParams->acvo_acm = (0x1 & pe_session->gLimEdcaParamsBC[3].aci.acm);
	pParams->acvo_aci = (0x3 & QCA_WLAN_AC_VO);
	pParams->acvo_acwmin =
		(0xf & pe_session->gLimEdcaParamsBC[3].cw.min);
	pParams->acvo_acwmax =
		(0xf & pe_session->gLimEdcaParamsBC[3].cw.max);
	pParams->acvo_txoplimit = pe_session->gLimEdcaParamsBC[3].txoplimit;

	pParams->present = 1;

} /* End populate_dot11f_wmm_params. */

QDF_STATUS
populate_dot11f_wpa(struct mac_context *mac,
		    tpSirRSNie pRsnIe, tDot11fIEWPA *pDot11f)
{
	uint32_t status;
	int idx;

	if (pRsnIe->length) {
		idx = find_ie_location(mac, pRsnIe, DOT11F_EID_WPA);
		if (0 <= idx) {
			status = dot11f_unpack_ie_wpa(mac, pRsnIe->rsnIEdata + idx + 2 + 4,       /* EID, length, OUI */
						      pRsnIe->rsnIEdata[idx + 1] - 4,   /* OUI */
						      pDot11f, false);
			if (DOT11F_FAILED(status)) {
				pe_err("Parse failure in Populate Dot11fWPA (0x%08x)",
					status);
				return QDF_STATUS_E_FAILURE;
			}
		}
	}

	return QDF_STATUS_SUCCESS;
} /* End populate_dot11f_wpa. */

QDF_STATUS populate_dot11f_wpa_opaque(struct mac_context *mac,
					 tpSirRSNie pRsnIe,
					 tDot11fIEWPAOpaque *pDot11f)
{
	int idx;

	if (pRsnIe->length) {
		idx = find_ie_location(mac, pRsnIe, DOT11F_EID_WPA);
		if (0 <= idx) {
			pDot11f->present = 1;
			pDot11f->num_data = pRsnIe->rsnIEdata[idx + 1] - 4;
			qdf_mem_copy(pDot11f->data, pRsnIe->rsnIEdata + idx + 2 + 4,    /* EID, len, OUI */
				     pRsnIe->rsnIEdata[idx + 1] - 4);   /* OUI */
		}
	}

	return QDF_STATUS_SUCCESS;

} /* End populate_dot11f_wpa_opaque. */

/* ////////////////////////////////////////////////////////////////////// */

QDF_STATUS
sir_convert_probe_req_frame2_struct(struct mac_context *mac,
				    uint8_t *pFrame,
				    uint32_t nFrame, tpSirProbeReq pProbeReq)
{
	uint32_t status;
	tDot11fProbeRequest pr;

	/* Ok, zero-init our [out] parameter, */
	qdf_mem_zero((uint8_t *) pProbeReq, sizeof(tSirProbeReq));

	/* delegate to the framesc-generated code, */
	status = dot11f_unpack_probe_request(mac, pFrame, nFrame, &pr, false);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to parse a Probe Request (0x%08x, %d bytes):",
			status, nFrame);
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_ERROR,
				   pFrame, nFrame);
		return QDF_STATUS_E_FAILURE;
	} else if (DOT11F_WARNED(status)) {
		pe_debug("There were warnings while unpacking a Probe Request (0x%08x, %d bytes):",
			status, nFrame);
	}
	/* & "transliterate" from a 'tDot11fProbeRequestto' a 'tSirProbeReq'... */
	if (!pr.SSID.present) {
		pe_debug("Mandatory IE SSID not present!");
	} else {
		pProbeReq->ssidPresent = 1;
		convert_ssid(mac, &pProbeReq->ssId, &pr.SSID);
	}

	if (!pr.SuppRates.present) {
		pe_debug_rl("Mandatory IE Supported Rates not present!");
		return QDF_STATUS_E_FAILURE;
	} else {
		pProbeReq->suppRatesPresent = 1;
		convert_supp_rates(mac, &pProbeReq->supportedRates,
				   &pr.SuppRates);
	}

	if (pr.ExtSuppRates.present) {
		pProbeReq->extendedRatesPresent = 1;
		convert_ext_supp_rates(mac, &pProbeReq->extendedRates,
				       &pr.ExtSuppRates);
	}

	if (pr.HTCaps.present) {
		qdf_mem_copy(&pProbeReq->HTCaps, &pr.HTCaps,
			     sizeof(tDot11fIEHTCaps));
	}

	if (pr.WscProbeReq.present) {
		pProbeReq->wscIePresent = 1;
		memcpy(&pProbeReq->probeReqWscIeInfo, &pr.WscProbeReq,
		       sizeof(tDot11fIEWscProbeReq));
	}
	if (pr.VHTCaps.present) {
		qdf_mem_copy(&pProbeReq->VHTCaps, &pr.VHTCaps,
			     sizeof(tDot11fIEVHTCaps));
	}
	if (pr.P2PProbeReq.present) {
		pProbeReq->p2pIePresent = 1;
	}
	if (pr.he_cap.present) {
		qdf_mem_copy(&pProbeReq->he_cap, &pr.he_cap,
			     sizeof(tDot11fIEhe_cap));
	}
	return QDF_STATUS_SUCCESS;
} /* End sir_convert_probe_req_frame2_struct. */


/**
 * sir_validate_and_rectify_ies() - API to check malformed frame
 * @mac_ctx: mac context
 * @mgmt_frame: pointer to management frame
 * @frame_bytes: no of bytes in frame
 * @missing_rsn_bytes: missing rsn bytes
 *
 * The frame would contain fixed IEs of 12 bytes followed by variable IEs
 * (Tagged elements). Every Tagged IE has tag number, tag length and data.
 * Tag length indicates the size of data in bytes.
 * This function checks for size of Frame received with the sum of all IEs.
 * And also rectifies missing optional fields in IE.
 *
 * NOTE : Presently this function rectifies RSN capability in RSN IE, can
 * be extended to rectify other optional fields in other IEs.
 *
 * Return: 0 on success, error number otherwise.
 */
QDF_STATUS
sir_validate_and_rectify_ies(struct mac_context *mac_ctx,
				uint8_t *mgmt_frame,
				uint32_t frame_bytes,
				uint32_t *missing_rsn_bytes)
{
	uint32_t length = SIZE_OF_FIXED_PARAM;
	uint8_t *ref_frame = NULL;

	/* Frame contains atleast one IE */
	if (frame_bytes > (SIZE_OF_FIXED_PARAM +
			SIZE_OF_TAG_PARAM_NUM + SIZE_OF_TAG_PARAM_LEN)) {
		while (length < frame_bytes) {
			/* ref frame points to next IE */
			ref_frame = mgmt_frame + length;
			length += (uint32_t)(SIZE_OF_TAG_PARAM_NUM +
					SIZE_OF_TAG_PARAM_LEN +
					(*(ref_frame + SIZE_OF_TAG_PARAM_NUM)));
		}
		if (length != frame_bytes) {
			/*
			 * Workaround : Some APs may not include RSN
			 * Capability but the length of which is included in
			 * RSN IE length. This may cause in updating RSN
			 * Capability with junk value. To avoid this, add RSN
			 * Capability value with default value.
			 */
			if (ref_frame && (*ref_frame == RSNIEID) &&
				(length == (frame_bytes +
					RSNIE_CAPABILITY_LEN))) {
				/* Assume RSN Capability as 00 */
				qdf_mem_set((uint8_t *)(mgmt_frame +
					(frame_bytes)),
					RSNIE_CAPABILITY_LEN,
					DEFAULT_RSNIE_CAP_VAL);
				*missing_rsn_bytes = RSNIE_CAPABILITY_LEN;
				pe_debug("Added RSN Capability to RSNIE as 0x00 0x00");
				return QDF_STATUS_SUCCESS;
			}
			return QDF_STATUS_E_FAILURE;
		}
	}
	return QDF_STATUS_SUCCESS;
}

void sir_copy_caps_info(struct mac_context *mac_ctx, tDot11fFfCapabilities caps,
					    tpSirProbeRespBeacon pProbeResp)
{
	pProbeResp->capabilityInfo.ess = caps.ess;
	pProbeResp->capabilityInfo.ibss = caps.ibss;
	pProbeResp->capabilityInfo.cfPollable = caps.cfPollable;
	pProbeResp->capabilityInfo.cfPollReq = caps.cfPollReq;
	pProbeResp->capabilityInfo.privacy = caps.privacy;
	pProbeResp->capabilityInfo.shortPreamble = caps.shortPreamble;
	pProbeResp->capabilityInfo.pbcc = caps.pbcc;
	pProbeResp->capabilityInfo.channelAgility =	caps.channelAgility;
	pProbeResp->capabilityInfo.spectrumMgt = caps.spectrumMgt;
	pProbeResp->capabilityInfo.qos = caps.qos;
	pProbeResp->capabilityInfo.shortSlotTime = caps.shortSlotTime;
	pProbeResp->capabilityInfo.apsd = caps.apsd;
	pProbeResp->capabilityInfo.rrm = caps.rrm;
	pProbeResp->capabilityInfo.dsssOfdm = caps.dsssOfdm;
	pProbeResp->capabilityInfo.delayedBA = caps.delayedBA;
	pProbeResp->capabilityInfo.immediateBA = caps.immediateBA;
}

#ifdef WLAN_FEATURE_FILS_SK
static void populate_dot11f_fils_rsn(struct mac_context *mac_ctx,
				     tDot11fIERSNOpaque *p_dot11f,
				     uint8_t *rsn_ie)
{
	pe_debug("FILS RSN IE length %d", rsn_ie[1]);
	if (rsn_ie[1]) {
		p_dot11f->present = 1;
		p_dot11f->num_data = rsn_ie[1];
		qdf_mem_copy(p_dot11f->data, &rsn_ie[2], rsn_ie[1]);
	}
}

void populate_dot11f_fils_params(struct mac_context *mac_ctx,
		tDot11fAssocRequest *frm,
		struct pe_session *pe_session)
{
	struct pe_fils_session *fils_info = pe_session->fils_info;

	/* Populate RSN IE with FILS AKM */
	populate_dot11f_fils_rsn(mac_ctx, &frm->RSNOpaque,
				 fils_info->rsn_ie);

	/* Populate FILS session IE */
	frm->fils_session.present = true;
	qdf_mem_copy(frm->fils_session.session,
		     fils_info->fils_session, FILS_SESSION_LENGTH);

	/* Populate FILS Key confirmation IE */
	if (fils_info->key_auth_len) {
		frm->fils_key_confirmation.present = true;
		frm->fils_key_confirmation.num_key_auth =
						fils_info->key_auth_len;

		qdf_mem_copy(frm->fils_key_confirmation.key_auth,
			     fils_info->key_auth, fils_info->key_auth_len);
	}
}

/**
 * update_fils_data: update fils params from beacon/probe response
 * @fils_ind: pointer to sir_fils_indication
 * @fils_indication: pointer to tDot11fIEfils_indication
 *
 * Return: None
 */
void update_fils_data(struct sir_fils_indication *fils_ind,
		      tDot11fIEfils_indication *fils_indication)
{
	uint8_t *data;
	uint8_t remaining_data = fils_indication->num_variable_data;

	data = fils_indication->variable_data;
	fils_ind->is_present = true;
	fils_ind->is_ip_config_supported =
			fils_indication->is_ip_config_supported;
	fils_ind->is_fils_sk_auth_supported =
			fils_indication->is_fils_sk_auth_supported;
	fils_ind->is_fils_sk_auth_pfs_supported =
			fils_indication->is_fils_sk_auth_pfs_supported;
	fils_ind->is_pk_auth_supported =
			fils_indication->is_pk_auth_supported;
	if (fils_indication->is_cache_id_present) {
		if (remaining_data < SIR_CACHE_IDENTIFIER_LEN) {
			pe_err("Failed to copy Cache Identifier, Invalid remaining data %d",
				remaining_data);
			return;
		}
		fils_ind->cache_identifier.is_present = true;
		qdf_mem_copy(fils_ind->cache_identifier.identifier,
				data, SIR_CACHE_IDENTIFIER_LEN);
		data = data + SIR_CACHE_IDENTIFIER_LEN;
		remaining_data = remaining_data - SIR_CACHE_IDENTIFIER_LEN;
	}
	if (fils_indication->is_hessid_present) {
		if (remaining_data < SIR_HESSID_LEN) {
			pe_err("Failed to copy HESSID, Invalid remaining data %d",
				remaining_data);
			return;
		}
		fils_ind->hessid.is_present = true;
		qdf_mem_copy(fils_ind->hessid.hessid,
				data, SIR_HESSID_LEN);
		data = data + SIR_HESSID_LEN;
		remaining_data = remaining_data - SIR_HESSID_LEN;
	}
	if (fils_indication->realm_identifiers_cnt) {
		if (remaining_data < (fils_indication->realm_identifiers_cnt *
		    SIR_REALM_LEN)) {
			pe_err("Failed to copy Realm Identifier, Invalid remaining data %d realm_cnt %d",
				remaining_data,
				fils_indication->realm_identifiers_cnt);
			return;
		}
		fils_ind->realm_identifier.is_present = true;
		fils_ind->realm_identifier.realm_cnt =
			fils_indication->realm_identifiers_cnt;
		qdf_mem_copy(fils_ind->realm_identifier.realm,
			data, fils_ind->realm_identifier.realm_cnt *
					SIR_REALM_LEN);
	}
}
#endif

#ifdef WLAN_FEATURE_11AX_BSS_COLOR
static void update_bss_color_change_ie_from_probe_rsp(
		tDot11fProbeResponse *prb_frm,
		tpSirProbeRespBeacon prb_rsp_struct)
{
	if (prb_frm->bss_color_change.present) {
		pe_debug("11AX: HE BSS color change present");
		qdf_mem_copy(&prb_rsp_struct->vendor_he_bss_color_change,
			     &prb_frm->bss_color_change,
			     sizeof(tDot11fIEbss_color_change));
	}
}
#else
static inline void update_bss_color_change_ie_from_probe_rsp(
		tDot11fProbeResponse *prb_frm,
		tpSirProbeRespBeacon prb_rsp_struct)
{}
#endif
QDF_STATUS sir_convert_probe_frame2_struct(struct mac_context *mac,
					      uint8_t *pFrame,
					      uint32_t nFrame,
					      tpSirProbeRespBeacon pProbeResp)
{
	uint32_t status;
	tDot11fProbeResponse *pr;

	/* Ok, zero-init our [out] parameter, */
	qdf_mem_zero((uint8_t *) pProbeResp, sizeof(tSirProbeRespBeacon));

	pr = qdf_mem_malloc(sizeof(tDot11fProbeResponse));
	if (!pr)
		return QDF_STATUS_E_NOMEM;

	/* delegate to the framesc-generated code, */
	status = dot11f_unpack_probe_response(mac, pFrame, nFrame, pr, false);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to parse a Probe Response (0x%08x, %d bytes):",
			status, nFrame);
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
				   pFrame, nFrame);
		qdf_mem_free(pr);
		return QDF_STATUS_E_FAILURE;
	}
	/* & "transliterate" from a 'tDot11fProbeResponse' to a 'tSirProbeRespBeacon'... */

	/* Timestamp */
	qdf_mem_copy((uint8_t *) pProbeResp->timeStamp,
		     (uint8_t *) &pr->TimeStamp, sizeof(tSirMacTimeStamp));

	/* Beacon Interval */
	pProbeResp->beaconInterval = pr->BeaconInterval.interval;

	sir_copy_caps_info(mac, pr->Capabilities, pProbeResp);

	if (!pr->SSID.present) {
		pe_debug("Mandatory IE SSID not present!");
	} else {
		pProbeResp->ssidPresent = 1;
		convert_ssid(mac, &pProbeResp->ssId, &pr->SSID);
	}

	if (!pr->SuppRates.present) {
		pe_debug_rl("Mandatory IE Supported Rates not present!");
	} else {
		pProbeResp->suppRatesPresent = 1;
		convert_supp_rates(mac, &pProbeResp->supportedRates,
				   &pr->SuppRates);
	}

	if (pr->ExtSuppRates.present) {
		pProbeResp->extendedRatesPresent = 1;
		convert_ext_supp_rates(mac, &pProbeResp->extendedRates,
				       &pr->ExtSuppRates);
	}

	if (pr->CFParams.present) {
		pProbeResp->cfPresent = 1;
		convert_cf_params(mac, &pProbeResp->cfParamSet, &pr->CFParams);
	}

	if (pr->Country.present) {
		pProbeResp->countryInfoPresent = 1;
		convert_country(mac, &pProbeResp->countryInfoParam,
				&pr->Country);
	}

	if (pr->EDCAParamSet.present) {
		pProbeResp->edcaPresent = 1;
		convert_edca_param(mac, &pProbeResp->edcaParams,
				   &pr->EDCAParamSet);
	}

	if (pr->ChanSwitchAnn.present) {
		pProbeResp->channelSwitchPresent = 1;
		qdf_mem_copy(&pProbeResp->channelSwitchIE, &pr->ChanSwitchAnn,
			     sizeof(pProbeResp->channelSwitchIE));
	}

	if (pr->ext_chan_switch_ann.present) {
		pProbeResp->ext_chan_switch_present = 1;
		qdf_mem_copy(&pProbeResp->ext_chan_switch,
			     &pr->ext_chan_switch_ann,
			     sizeof(tDot11fIEext_chan_switch_ann));
	}

	if (pr->SuppOperatingClasses.present) {
		pProbeResp->supp_operating_class_present = 1;
		qdf_mem_copy(&pProbeResp->supp_operating_classes,
			&pr->SuppOperatingClasses,
			sizeof(tDot11fIESuppOperatingClasses));
	}

	if (pr->sec_chan_offset_ele.present) {
		pProbeResp->sec_chan_offset_present = 1;
		qdf_mem_copy(&pProbeResp->sec_chan_offset,
			     &pr->sec_chan_offset_ele,
			     sizeof(pProbeResp->sec_chan_offset));
	}

	if (pr->TPCReport.present) {
		pProbeResp->tpcReportPresent = 1;
		qdf_mem_copy(&pProbeResp->tpcReport, &pr->TPCReport,
			     sizeof(tDot11fIETPCReport));
	}

	if (pr->PowerConstraints.present) {
		pProbeResp->powerConstraintPresent = 1;
		qdf_mem_copy(&pProbeResp->localPowerConstraint,
			     &pr->PowerConstraints,
			     sizeof(tDot11fIEPowerConstraints));
	}

	if (pr->Quiet.present) {
		pProbeResp->quietIEPresent = 1;
		qdf_mem_copy(&pProbeResp->quietIE, &pr->Quiet,
			     sizeof(tDot11fIEQuiet));
	}

	if (pr->HTCaps.present) {
		qdf_mem_copy(&pProbeResp->HTCaps, &pr->HTCaps,
			     sizeof(tDot11fIEHTCaps));
	}

	if (pr->HTInfo.present) {
		qdf_mem_copy(&pProbeResp->HTInfo, &pr->HTInfo,
			     sizeof(tDot11fIEHTInfo));
	}

	if (pr->he_op.oper_info_6g_present) {
		pProbeResp->chan_freq = wlan_reg_chan_band_to_freq(mac->pdev,
						pr->he_op.oper_info_6g.info.primary_ch,
						BIT(REG_BAND_6G));
	} else if (pr->DSParams.present) {
		pProbeResp->dsParamsPresent = 1;
		pProbeResp->chan_freq =
		    wlan_reg_legacy_chan_to_freq(mac->pdev, pr->DSParams.curr_channel);
	} else if (pr->HTInfo.present) {
		pProbeResp->chan_freq =
		    wlan_reg_legacy_chan_to_freq(mac->pdev, pr->HTInfo.primaryChannel);
	}

	if (pr->RSNOpaque.present) {
		pProbeResp->rsnPresent = 1;
		convert_rsn_opaque(mac, &pProbeResp->rsn, &pr->RSNOpaque);
	}

	if (pr->WPA.present) {
		pProbeResp->wpaPresent = 1;
		convert_wpa(mac, &pProbeResp->wpa, &pr->WPA);
	}

	if (pr->WMMParams.present) {
		pProbeResp->wmeEdcaPresent = 1;
		convert_wmm_params(mac, &pProbeResp->edcaParams, &pr->WMMParams);
	}

	if (pr->WMMInfoAp.present) {
		pProbeResp->wmeInfoPresent = 1;
		pe_debug("WMM Information Element present in Probe Response Frame!");
	}

	if (pr->WMMCaps.present) {
		pProbeResp->wsmCapablePresent = 1;
	}

	if (pr->ERPInfo.present) {
		pProbeResp->erpPresent = 1;
		convert_erp_info(mac, &pProbeResp->erpIEInfo, &pr->ERPInfo);
	}
	if (pr->MobilityDomain.present) {
		/* MobilityDomain */
		pProbeResp->mdiePresent = 1;
		qdf_mem_copy((uint8_t *) &(pProbeResp->mdie[0]),
			     (uint8_t *) &(pr->MobilityDomain.MDID),
			     sizeof(uint16_t));
		pProbeResp->mdie[2] =
			((pr->MobilityDomain.overDSCap << 0) | (pr->MobilityDomain.
								resourceReqCap <<
								1));
		pe_debug("mdie=%02x%02x%02x",
			(unsigned int)pProbeResp->mdie[0],
			(unsigned int)pProbeResp->mdie[1],
			(unsigned int)pProbeResp->mdie[2]);
	}

#if defined FEATURE_WLAN_ESE
	if (pr->ESEVersion.present)
		pProbeResp->is_ese_ver_ie_present = 1;
	if (pr->QBSSLoad.present) {
		qdf_mem_copy(&pProbeResp->QBSSLoad, &pr->QBSSLoad,
			     sizeof(tDot11fIEQBSSLoad));
	}
#endif
	if (pr->P2PProbeRes.present) {
		qdf_mem_copy(&pProbeResp->P2PProbeRes, &pr->P2PProbeRes,
			     sizeof(tDot11fIEP2PProbeRes));
	}
	if (pr->VHTCaps.present) {
		qdf_mem_copy(&pProbeResp->VHTCaps, &pr->VHTCaps,
			     sizeof(tDot11fIEVHTCaps));
	}
	if (pr->VHTOperation.present) {
		qdf_mem_copy(&pProbeResp->VHTOperation, &pr->VHTOperation,
			     sizeof(tDot11fIEVHTOperation));
	}
	if (pr->VHTExtBssLoad.present) {
		qdf_mem_copy(&pProbeResp->VHTExtBssLoad, &pr->VHTExtBssLoad,
			     sizeof(tDot11fIEVHTExtBssLoad));
	}
	pProbeResp->Vendor1IEPresent = pr->Vendor1IE.present;
	pProbeResp->Vendor3IEPresent = pr->Vendor3IE.present;

	pProbeResp->vendor_vht_ie.present = pr->vendor_vht_ie.present;
	if (pr->vendor_vht_ie.present)
		pProbeResp->vendor_vht_ie.sub_type = pr->vendor_vht_ie.sub_type;
	if (pr->vendor_vht_ie.VHTCaps.present) {
		qdf_mem_copy(&pProbeResp->vendor_vht_ie.VHTCaps,
				&pr->vendor_vht_ie.VHTCaps,
				sizeof(tDot11fIEVHTCaps));
	}
	if (pr->vendor_vht_ie.VHTOperation.present) {
		qdf_mem_copy(&pProbeResp->vendor_vht_ie.VHTOperation,
				&pr->vendor_vht_ie.VHTOperation,
				sizeof(tDot11fIEVHTOperation));
	}
	/* Update HS 2.0 Information Element */
	if (pr->hs20vendor_ie.present) {
		pe_debug("HS20 Indication Element Present, rel#:%u, id:%u",
			pr->hs20vendor_ie.release_num,
			pr->hs20vendor_ie.hs_id_present);
		qdf_mem_copy(&pProbeResp->hs20vendor_ie,
			&pr->hs20vendor_ie,
			sizeof(tDot11fIEhs20vendor_ie) -
			sizeof(pr->hs20vendor_ie.hs_id));
		if (pr->hs20vendor_ie.hs_id_present)
			qdf_mem_copy(&pProbeResp->hs20vendor_ie.hs_id,
				&pr->hs20vendor_ie.hs_id,
				sizeof(pr->hs20vendor_ie.hs_id));
	}
	if (pr->MBO_IE.present) {
		pProbeResp->MBO_IE_present = true;
		if (pr->MBO_IE.cellular_data_cap.present)
			pProbeResp->MBO_capability =
				pr->MBO_IE.cellular_data_cap.cellular_connectivity;

		if (pr->MBO_IE.assoc_disallowed.present) {
			pProbeResp->assoc_disallowed = true;
			pProbeResp->assoc_disallowed_reason =
				pr->MBO_IE.assoc_disallowed.reason_code;
		}
	}

	if (pr->qcn_ie.present)
		qdf_mem_copy(&pProbeResp->qcn_ie, &pr->qcn_ie,
			     sizeof(tDot11fIEqcn_ie));

	if (pr->he_cap.present) {
		qdf_mem_copy(&pProbeResp->he_cap, &pr->he_cap,
			     sizeof(tDot11fIEhe_cap));
	}
	if (pr->he_op.present) {
		qdf_mem_copy(&pProbeResp->he_op, &pr->he_op,
			     sizeof(tDot11fIEhe_op));
	}

	update_bss_color_change_ie_from_probe_rsp(pr, pProbeResp);

	qdf_mem_free(pr);
	return QDF_STATUS_SUCCESS;

} /* End sir_convert_probe_frame2_struct. */

QDF_STATUS
sir_convert_assoc_req_frame2_struct(struct mac_context *mac,
				    uint8_t *pFrame,
				    uint32_t nFrame, tpSirAssocReq pAssocReq)
{
	tDot11fAssocRequest *ar;
	uint32_t status;

	ar = qdf_mem_malloc(sizeof(tDot11fAssocRequest));
	if (!ar)
		return QDF_STATUS_E_NOMEM;
	/* Zero-init our [out] parameter, */
	qdf_mem_zero((uint8_t *) pAssocReq, sizeof(tSirAssocReq));

	/* delegate to the framesc-generated code, */
	status = dot11f_unpack_assoc_request(mac, pFrame, nFrame, ar, false);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to parse an Association Request (0x%08x, %d bytes):",
			status, nFrame);
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_ERROR,
				   pFrame, nFrame);
		qdf_mem_free(ar);
		return QDF_STATUS_E_FAILURE;
	} else if (DOT11F_WARNED(status)) {
		pe_debug("There were warnings while unpacking an Assoication Request (0x%08x, %d bytes):",
			status, nFrame);
	}
	/* & "transliterate" from a 'tDot11fAssocRequest' to a 'tSirAssocReq'... */

	/* make sure this is seen as an assoc request */
	pAssocReq->reassocRequest = 0;

	/* Capabilities */
	pAssocReq->capabilityInfo.ess = ar->Capabilities.ess;
	pAssocReq->capabilityInfo.ibss = ar->Capabilities.ibss;
	pAssocReq->capabilityInfo.cfPollable = ar->Capabilities.cfPollable;
	pAssocReq->capabilityInfo.cfPollReq = ar->Capabilities.cfPollReq;
	pAssocReq->capabilityInfo.privacy = ar->Capabilities.privacy;
	pAssocReq->capabilityInfo.shortPreamble =
		ar->Capabilities.shortPreamble;
	pAssocReq->capabilityInfo.pbcc = ar->Capabilities.pbcc;
	pAssocReq->capabilityInfo.channelAgility =
		ar->Capabilities.channelAgility;
	pAssocReq->capabilityInfo.spectrumMgt = ar->Capabilities.spectrumMgt;
	pAssocReq->capabilityInfo.qos = ar->Capabilities.qos;
	pAssocReq->capabilityInfo.shortSlotTime =
		ar->Capabilities.shortSlotTime;
	pAssocReq->capabilityInfo.apsd = ar->Capabilities.apsd;
	pAssocReq->capabilityInfo.rrm = ar->Capabilities.rrm;
	pAssocReq->capabilityInfo.dsssOfdm = ar->Capabilities.dsssOfdm;
	pAssocReq->capabilityInfo.delayedBA = ar->Capabilities.delayedBA;
	pAssocReq->capabilityInfo.immediateBA = ar->Capabilities.immediateBA;

	/* Listen Interval */
	pAssocReq->listenInterval = ar->ListenInterval.interval;

	/* SSID */
	if (ar->SSID.present) {
		pAssocReq->ssidPresent = 1;
		convert_ssid(mac, &pAssocReq->ssId, &ar->SSID);
	}
	/* Supported Rates */
	if (ar->SuppRates.present) {
		pAssocReq->suppRatesPresent = 1;
		convert_supp_rates(mac, &pAssocReq->supportedRates,
				   &ar->SuppRates);
	}
	/* Extended Supported Rates */
	if (ar->ExtSuppRates.present) {
		pAssocReq->extendedRatesPresent = 1;
		convert_ext_supp_rates(mac, &pAssocReq->extendedRates,
				       &ar->ExtSuppRates);
	}
	/* QOS Capabilities: */
	if (ar->QOSCapsStation.present) {
		pAssocReq->qosCapabilityPresent = 1;
		convert_qos_caps_station(mac, &pAssocReq->qosCapability,
					 &ar->QOSCapsStation);
	}
	/* WPA */
	if (ar->WPAOpaque.present) {
		pAssocReq->wpaPresent = 1;
		convert_wpa_opaque(mac, &pAssocReq->wpa, &ar->WPAOpaque);
	}
#ifdef FEATURE_WLAN_WAPI
	if (ar->WAPIOpaque.present) {
		pAssocReq->wapiPresent = 1;
		convert_wapi_opaque(mac, &pAssocReq->wapi, &ar->WAPIOpaque);
	}
#endif
	/* RSN */
	if (ar->RSNOpaque.present) {
		pAssocReq->rsnPresent = 1;
		convert_rsn_opaque(mac, &pAssocReq->rsn, &ar->RSNOpaque);
	}
	/* WSC IE */
	if (ar->WscIEOpaque.present) {
		pAssocReq->addIEPresent = 1;
		convert_wsc_opaque(mac, &pAssocReq->addIE, &ar->WscIEOpaque);
	}

	if (ar->P2PIEOpaque.present) {
		pAssocReq->addIEPresent = 1;
		convert_p2p_opaque(mac, &pAssocReq->addIE, &ar->P2PIEOpaque);
	}
#ifdef WLAN_FEATURE_WFD
	if (ar->WFDIEOpaque.present) {
		pAssocReq->addIEPresent = 1;
		convert_wfd_opaque(mac, &pAssocReq->addIE, &ar->WFDIEOpaque);
	}
#endif

	/* Power Capabilities */
	if (ar->PowerCaps.present) {
		pAssocReq->powerCapabilityPresent = 1;
		convert_power_caps(mac, &pAssocReq->powerCapability,
				   &ar->PowerCaps);
	}
	/* Supported Channels */
	if (ar->SuppChannels.present) {
		pAssocReq->supportedChannelsPresent = 1;
		convert_supp_channels(mac, &pAssocReq->supportedChannels,
				      &ar->SuppChannels);
	}

	if (ar->HTCaps.present) {
		qdf_mem_copy(&pAssocReq->HTCaps, &ar->HTCaps,
			     sizeof(tDot11fIEHTCaps));
	}

	if (ar->WMMInfoStation.present) {
		pAssocReq->wmeInfoPresent = 1;
		qdf_mem_copy(&pAssocReq->WMMInfoStation, &ar->WMMInfoStation,
			     sizeof(tDot11fIEWMMInfoStation));

	}

	if (ar->WMMCaps.present)
		pAssocReq->wsmCapablePresent = 1;

	if (!pAssocReq->ssidPresent) {
		pe_debug("Received Assoc without SSID IE");
		qdf_mem_free(ar);
		return QDF_STATUS_E_FAILURE;
	}

	if (!pAssocReq->suppRatesPresent && !pAssocReq->extendedRatesPresent) {
		pe_debug("Received Assoc without supp rate IE");
		qdf_mem_free(ar);
		return QDF_STATUS_E_FAILURE;
	}
	if (ar->VHTCaps.present) {
		qdf_mem_copy(&pAssocReq->VHTCaps, &ar->VHTCaps,
			     sizeof(tDot11fIEVHTCaps));
		pe_debug("Received Assoc Req with VHT Cap");
		lim_log_vht_cap(mac, &pAssocReq->VHTCaps);
	}
	if (ar->OperatingMode.present) {
		qdf_mem_copy(&pAssocReq->operMode, &ar->OperatingMode,
			     sizeof(tDot11fIEOperatingMode));
		pe_debug("Received Assoc Req with Operating Mode IE");
		lim_log_operating_mode(mac, &pAssocReq->operMode);
	}
	if (ar->ExtCap.present) {
		struct s_ext_cap *ext_cap;

		qdf_mem_copy(&pAssocReq->ExtCap, &ar->ExtCap,
			    sizeof(tDot11fIEExtCap));
		ext_cap = (struct s_ext_cap *)&pAssocReq->ExtCap.bytes;
		pe_debug("timingMeas: %d, finetimingMeas Init: %d, Resp: %d",
			ext_cap->timing_meas, ext_cap->fine_time_meas_initiator,
			ext_cap->fine_time_meas_responder);
	}
	if (ar->SuppOperatingClasses.present) {
		uint8_t num_classes = ar->SuppOperatingClasses.num_classes;

		if (num_classes > sizeof(ar->SuppOperatingClasses.classes))
			num_classes =
				sizeof(ar->SuppOperatingClasses.classes);
		qdf_mem_copy(&pAssocReq->supp_operating_classes,
			     &ar->SuppOperatingClasses,
			     sizeof(tDot11fIESuppOperatingClasses));
		QDF_TRACE_HEX_DUMP(
			QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			ar->SuppOperatingClasses.classes, num_classes);
	}

	pAssocReq->vendor_vht_ie.present = ar->vendor_vht_ie.present;
	if (ar->vendor_vht_ie.present) {
		pAssocReq->vendor_vht_ie.sub_type = ar->vendor_vht_ie.sub_type;
		if (ar->vendor_vht_ie.VHTCaps.present) {
			qdf_mem_copy(&pAssocReq->vendor_vht_ie.VHTCaps,
				     &ar->vendor_vht_ie.VHTCaps,
				     sizeof(tDot11fIEVHTCaps));
			pe_debug("Received Assoc Request with Vendor specific VHT Cap");
			lim_log_vht_cap(mac, &pAssocReq->VHTCaps);
		}
	}
	if (ar->qcn_ie.present)
		qdf_mem_copy(&pAssocReq->qcn_ie, &ar->qcn_ie,
			     sizeof(tDot11fIEqcn_ie));
	if (ar->he_cap.present) {
		qdf_mem_copy(&pAssocReq->he_cap, &ar->he_cap,
			     sizeof(tDot11fIEhe_cap));
		pe_debug("Received Assoc Req with HE Capability IE");
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
				   &pAssocReq->he_cap, sizeof(tDot11fIEhe_cap));
	}
	if (ar->he_6ghz_band_cap.present) {
		qdf_mem_copy(&pAssocReq->he_6ghz_band_cap,
			     &ar->he_6ghz_band_cap,
			     sizeof(tDot11fIEhe_6ghz_band_cap));
		pe_debug("Received Assoc Req with HE Band Capability IE");
	}
	qdf_mem_free(ar);
	return QDF_STATUS_SUCCESS;

} /* End sir_convert_assoc_req_frame2_struct. */

/**
 * dot11f_parse_assoc_response() - API to parse Assoc IE buffer to struct
 * @mac_ctx: MAC context
 * @p_buf: Pointer to the assoc IE buffer
 * @n_buf: length of the @p_buf
 * @p_frm: Struct to populate the IE buffer after parsing
 * @append_ie: Boolean to indicate whether to reset @p_frm or not. If @append_ie
 *             is true, @p_frm struct is not reset to zeros.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dot11f_parse_assoc_response(struct mac_context *mac_ctx,
						 uint8_t *p_buf, uint32_t n_buf,
						 tDot11fAssocResponse *p_frm,
						 bool append_ie)
{
	uint32_t status;

	status = dot11f_unpack_assoc_response(mac_ctx, p_buf,
					      n_buf, p_frm, append_ie);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to parse an Association Response (0x%08x, %d bytes):",
			status, n_buf);
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_ERROR,
				   p_buf, n_buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_FILS_SK
/**
 * fils_convert_assoc_rsp_frame2_struct() - Copy FILS IE's to Assoc rsp struct
 * @ar: frame parser Assoc response struct
 * @pAssocRsp: LIM Assoc response
 *
 * Return: None
 */
static void fils_convert_assoc_rsp_frame2_struct(tDot11fAssocResponse *ar,
						 tpSirAssocRsp pAssocRsp)
{
	if (ar->fils_session.present) {
		pe_debug("fils session IE present");
		pAssocRsp->fils_session.present = true;
		qdf_mem_copy(pAssocRsp->fils_session.session,
				ar->fils_session.session,
				DOT11F_IE_FILS_SESSION_MAX_LEN);
	}

	if (ar->fils_key_confirmation.present) {
		pe_debug("fils key conf IE present");
		pAssocRsp->fils_key_auth.num_key_auth =
			ar->fils_key_confirmation.num_key_auth;
		qdf_mem_copy(pAssocRsp->fils_key_auth.key_auth,
				ar->fils_key_confirmation.key_auth,
				pAssocRsp->fils_key_auth.num_key_auth);
	}

	if (ar->fils_kde.present) {
		pe_debug("fils kde IE present %d",
				ar->fils_kde.num_kde_list);
		pAssocRsp->fils_kde.num_kde_list =
			ar->fils_kde.num_kde_list;
		qdf_mem_copy(pAssocRsp->fils_kde.key_rsc,
				ar->fils_kde.key_rsc, KEY_RSC_LEN);
		qdf_mem_copy(&pAssocRsp->fils_kde.kde_list,
				&ar->fils_kde.kde_list,
				pAssocRsp->fils_kde.num_kde_list);
	}

	if (ar->fils_hlp_container.present) {
		pe_debug("FILS HLP container IE present");
		sir_copy_mac_addr(pAssocRsp->dst_mac.bytes,
				ar->fils_hlp_container.dest_mac);
		sir_copy_mac_addr(pAssocRsp->src_mac.bytes,
				ar->fils_hlp_container.src_mac);
		pAssocRsp->hlp_data_len = ar->fils_hlp_container.num_hlp_packet;
		qdf_mem_copy(pAssocRsp->hlp_data,
				ar->fils_hlp_container.hlp_packet,
				pAssocRsp->hlp_data_len);

		if (ar->fragment_ie.present) {
			pe_debug("FILS fragment ie present");
			qdf_mem_copy(pAssocRsp->hlp_data +
					pAssocRsp->hlp_data_len,
					ar->fragment_ie.data,
					ar->fragment_ie.num_data);
			pAssocRsp->hlp_data_len += ar->fragment_ie.num_data;
		}
	}
}
#else
static inline void fils_convert_assoc_rsp_frame2_struct(tDot11fAssocResponse
							*ar, tpSirAssocRsp
							pAssocRsp)
{ }
#endif

QDF_STATUS wlan_parse_ftie_sha384(uint8_t *frame, uint32_t frame_len,
				  struct sSirAssocRsp *assoc_rsp)
{
	const uint8_t *ie, *ie_end, *pos;
	uint8_t ie_len, remaining_ie_len;
	struct wlan_sha384_ftinfo_subelem *ft_subelem;

	ie = wlan_get_ie_ptr_from_eid(DOT11F_EID_FTINFO, frame, frame_len);
	if (!ie) {
		pe_err("FT IE not present");
		return QDF_STATUS_E_FAILURE;
	}

	if (!ie[1]) {
		pe_err("FT IE length is zero");
		return QDF_STATUS_E_FAILURE;
	}

	ie_len = ie[1];
	if (ie_len < sizeof(struct wlan_sha384_ftinfo)) {
		pe_err("Invalid FTIE len:%d", ie_len);
		return QDF_STATUS_E_FAILURE;
	}
	remaining_ie_len = ie_len;
	pos = ie + 2;
	qdf_mem_copy(&assoc_rsp->sha384_ft_info, pos,
		     sizeof(struct wlan_sha384_ftinfo));
	ie_end = ie + ie_len;
	pos += sizeof(struct wlan_sha384_ftinfo);
	remaining_ie_len -= sizeof(struct wlan_sha384_ftinfo);
	ft_subelem = &assoc_rsp->sha384_ft_subelem;
	qdf_mem_zero(ft_subelem, sizeof(*ft_subelem));
	while (ie_end - pos >= 2) {
		uint8_t id, len;

		id = *pos++;
		len = *pos++;
		/* Subtract data length(len) + 1 bytes for
		 * Subelement ID + 1 bytes for length from
		 * remaining FTIE buffer len (ie_len).
		 * Subelement Parameter(s) field :
		 *         Subelement ID  Length     Data
		 * Octets:      1            1     variable
		 */
		if (len < 1 || remaining_ie_len < (len + 2)) {
			pe_err("Invalid FT subelem length");
			return QDF_STATUS_E_FAILURE;
		}

		remaining_ie_len -= (len + 2);

		switch (id) {
		case FTIE_SUBELEM_R1KH_ID:
			if (len != FTIE_R1KH_LEN) {
				pe_err("Invalid R1KH-ID length: %d",
				       len);
				return QDF_STATUS_E_FAILURE;
			}
			ft_subelem->r1kh_id.present = 1;
			qdf_mem_copy(ft_subelem->r1kh_id.PMK_R1_ID,
				     pos, FTIE_R1KH_LEN);
			break;
		case FTIE_SUBELEM_GTK:
			if (ft_subelem->gtk) {
				qdf_mem_zero(ft_subelem->gtk,
					     ft_subelem->gtk_len);
				ft_subelem->gtk_len = 0;
				qdf_mem_free(ft_subelem->gtk);
			}
			ft_subelem->gtk = qdf_mem_malloc(len);
			if (!ft_subelem->gtk)
				return QDF_STATUS_E_NOMEM;

			qdf_mem_copy(ft_subelem->gtk, pos, len);
			ft_subelem->gtk_len = len;
			break;
		case FTIE_SUBELEM_R0KH_ID:
			if (len < 1 || len > FTIE_R0KH_MAX_LEN) {
				pe_err("Invalid R0KH-ID length: %d",
				       len);
				return QDF_STATUS_E_FAILURE;
			}
			ft_subelem->r0kh_id.present = 1;
			ft_subelem->r0kh_id.num_PMK_R0_ID = len;
			qdf_mem_copy(ft_subelem->r0kh_id.PMK_R0_ID,
				     pos, len);
			break;
		case FTIE_SUBELEM_IGTK:
			if (ft_subelem->igtk) {
				qdf_mem_zero(ft_subelem->igtk,
					     ft_subelem->igtk_len);
				ft_subelem->igtk_len = 0;
				qdf_mem_free(ft_subelem->igtk);
			}
			ft_subelem->igtk = qdf_mem_malloc(len);
			if (!ft_subelem->igtk)
				return QDF_STATUS_E_NOMEM;

			qdf_mem_copy(ft_subelem->igtk, pos, len);
			ft_subelem->igtk_len = len;

			break;
		default:
			pe_debug("Unknown subelem id %d len:%d",
				 id, len);
			break;
		}
		pos += len;
	}
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
sir_convert_assoc_resp_frame2_struct(struct mac_context *mac,
				     struct pe_session *session_entry,
				     uint8_t *frame, uint32_t frame_len,
				     tpSirAssocRsp pAssocRsp)
{
	tDot11fAssocResponse *ar;
	enum ani_akm_type auth_type;
	uint32_t status, ie_len;
	QDF_STATUS qdf_status;
	uint8_t cnt = 0;
	bool sha384_akm;
	uint8_t *ie_ptr;
	uint16_t status_code;

	ar = qdf_mem_malloc(sizeof(*ar));
	if (!ar)
		return QDF_STATUS_E_FAILURE;

	status_code = sir_read_u16(frame +
				   SIR_MAC_ASSOC_RSP_STATUS_CODE_OFFSET);
	if (lim_is_fils_connection(session_entry) && status_code)
		pe_debug("FILS: assoc reject Status code:%d", status_code);

	/*
	 * decrypt the cipher text using AEAD decryption, if association
	 * response status code is successful, else the don't do AEAD decryption
	 * since AP doesn't inlude FILS session IE when association reject is
	 * sent
	 */
	if (lim_is_fils_connection(session_entry) && !status_code) {
		status = aead_decrypt_assoc_rsp(mac, session_entry,
						ar, frame, &frame_len);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			pe_err("FILS assoc rsp AEAD decrypt fails");
			qdf_mem_free(ar);
			return QDF_STATUS_E_FAILURE;
		}
	}

	status = dot11f_parse_assoc_response(mac, frame, frame_len, ar, false);
	if (QDF_STATUS_SUCCESS != status) {
		qdf_mem_free(ar);
		return status;
	}

	/* Capabilities */
	pAssocRsp->capabilityInfo.ess = ar->Capabilities.ess;
	pAssocRsp->capabilityInfo.ibss = ar->Capabilities.ibss;
	pAssocRsp->capabilityInfo.cfPollable = ar->Capabilities.cfPollable;
	pAssocRsp->capabilityInfo.cfPollReq = ar->Capabilities.cfPollReq;
	pAssocRsp->capabilityInfo.privacy = ar->Capabilities.privacy;
	pAssocRsp->capabilityInfo.shortPreamble =
		ar->Capabilities.shortPreamble;
	pAssocRsp->capabilityInfo.pbcc = ar->Capabilities.pbcc;
	pAssocRsp->capabilityInfo.channelAgility =
		ar->Capabilities.channelAgility;
	pAssocRsp->capabilityInfo.spectrumMgt = ar->Capabilities.spectrumMgt;
	pAssocRsp->capabilityInfo.qos = ar->Capabilities.qos;
	pAssocRsp->capabilityInfo.shortSlotTime =
		ar->Capabilities.shortSlotTime;
	pAssocRsp->capabilityInfo.apsd = ar->Capabilities.apsd;
	pAssocRsp->capabilityInfo.rrm = ar->Capabilities.rrm;
	pAssocRsp->capabilityInfo.dsssOfdm = ar->Capabilities.dsssOfdm;
	pAssocRsp->capabilityInfo.delayedBA = ar->Capabilities.delayedBA;
	pAssocRsp->capabilityInfo.immediateBA = ar->Capabilities.immediateBA;

	pAssocRsp->status_code = ar->Status.status;
	pAssocRsp->aid = ar->AID.associd;
#ifdef WLAN_FEATURE_11W
	if (ar->TimeoutInterval.present) {
		pAssocRsp->TimeoutInterval.present = 1;
		pAssocRsp->TimeoutInterval.timeoutType =
			ar->TimeoutInterval.timeoutType;
		pAssocRsp->TimeoutInterval.timeoutValue =
			ar->TimeoutInterval.timeoutValue;
	}
#endif

	if (!ar->SuppRates.present) {
		pAssocRsp->suppRatesPresent = 0;
		pe_debug_rl("Mandatory IE Supported Rates not present!");
	} else {
		pAssocRsp->suppRatesPresent = 1;
		convert_supp_rates(mac, &pAssocRsp->supportedRates,
				&ar->SuppRates);
	}

	if (ar->ExtSuppRates.present) {
		pAssocRsp->extendedRatesPresent = 1;
		convert_ext_supp_rates(mac, &pAssocRsp->extendedRates,
				&ar->ExtSuppRates);
	}

	if (ar->EDCAParamSet.present) {
		pAssocRsp->edcaPresent = 1;
		convert_edca_param(mac, &pAssocRsp->edca, &ar->EDCAParamSet);
	}
	if (ar->WMMParams.present) {
		pAssocRsp->wmeEdcaPresent = 1;
		convert_wmm_params(mac, &pAssocRsp->edca, &ar->WMMParams);
		pe_debug("Received Assoc Resp with WMM Param");
		__print_wmm_params(mac, &ar->WMMParams);
	}

	if (ar->HTCaps.present) {
		pe_debug("Received Assoc Resp with HT Cap");
		qdf_mem_copy(&pAssocRsp->HTCaps, &ar->HTCaps,
			     sizeof(tDot11fIEHTCaps));
	}

	if (ar->HTInfo.present) {
		pe_debug("Received Assoc Resp with HT Info");
		qdf_mem_copy(&pAssocRsp->HTInfo, &ar->HTInfo,
			     sizeof(tDot11fIEHTInfo));
	}
	if (ar->RRMEnabledCap.present) {
		qdf_mem_copy(&pAssocRsp->rrm_caps, &ar->RRMEnabledCap,
			     sizeof(tDot11fIERRMEnabledCap));
	}
	if (ar->MobilityDomain.present) {
		/* MobilityDomain */
		pAssocRsp->mdiePresent = 1;
		qdf_mem_copy((uint8_t *) &(pAssocRsp->mdie[0]),
				(uint8_t *) &(ar->MobilityDomain.MDID),
				sizeof(uint16_t));
		pAssocRsp->mdie[2] = ((ar->MobilityDomain.overDSCap << 0) |
				      (ar->MobilityDomain.resourceReqCap << 1));
		pe_debug("new mdie=%02x%02x%02x",
			(unsigned int)pAssocRsp->mdie[0],
			(unsigned int)pAssocRsp->mdie[1],
			(unsigned int)pAssocRsp->mdie[2]);
	}

	/*
	 * If the connection is based on SHA384 AKM suite,
	 * then the length of MIC is 24 bytes, but frame parser
	 * has FTIE MIC of 16 bytes only. This results in parsing FTIE
	 * failure and R0KH and R1KH are not sent to firmware over RSO
	 * command. Frame parser doesn't have
	 * info on the connected AKM. So parse the FTIE again if
	 * AKM is sha384 based and extract the R0KH and R1KH using the new
	 * parsing logic.
	 */
	auth_type = session_entry->connected_akm;
	sha384_akm = lim_is_sha384_akm(auth_type);
	ie_ptr = frame + FIXED_PARAM_OFFSET_ASSOC_RSP;
	ie_len = frame_len - FIXED_PARAM_OFFSET_ASSOC_RSP;
	if (sha384_akm) {
		qdf_status = wlan_parse_ftie_sha384(ie_ptr, ie_len, pAssocRsp);
		if (QDF_IS_STATUS_ERROR(qdf_status)) {
			pe_err("FT IE parsing failed status:%d", status);
		} else {
			pe_debug("FT: R0KH present:%d len:%d R1KH present%d",
				 pAssocRsp->sha384_ft_subelem.r0kh_id.present,
				 pAssocRsp->sha384_ft_subelem.r0kh_id.num_PMK_R0_ID,
				 pAssocRsp->sha384_ft_subelem.r1kh_id.present);
			ar->FTInfo.present = false;
		}
	} else if (ar->FTInfo.present) {
		pe_debug("FT: R0KH present:%d, len:%d R1KH present:%d",
			 ar->FTInfo.R0KH_ID.present,
			 ar->FTInfo.R0KH_ID.num_PMK_R0_ID,
			 ar->FTInfo.R1KH_ID.present);
		pAssocRsp->ftinfoPresent = 1;
		qdf_mem_copy(&pAssocRsp->FTInfo, &ar->FTInfo,
			     sizeof(tDot11fIEFTInfo));
	}

	if (ar->num_RICDataDesc && ar->num_RICDataDesc <= 2) {
		for (cnt = 0; cnt < ar->num_RICDataDesc; cnt++) {
			if (ar->RICDataDesc[cnt].present) {
				qdf_mem_copy(&pAssocRsp->RICData[cnt],
						&ar->RICDataDesc[cnt],
						sizeof(tDot11fIERICDataDesc));
			}
		}
		pAssocRsp->num_RICData = ar->num_RICDataDesc;
		pAssocRsp->ricPresent = true;
	}

#ifdef FEATURE_WLAN_ESE
	if (ar->num_WMMTSPEC) {
		pAssocRsp->num_tspecs = ar->num_WMMTSPEC;
		for (cnt = 0; cnt < ar->num_WMMTSPEC; cnt++) {
			qdf_mem_copy(&pAssocRsp->TSPECInfo[cnt],
					&ar->WMMTSPEC[cnt],
					sizeof(tDot11fIEWMMTSPEC));
		}
		pAssocRsp->tspecPresent = true;
	}

	if (ar->ESETrafStrmMet.present) {
		pAssocRsp->tsmPresent = 1;
		qdf_mem_copy(&pAssocRsp->tsmIE.tsid,
				&ar->ESETrafStrmMet.tsid,
				sizeof(struct ese_tsm_ie));
	}
#endif

	if (ar->VHTCaps.present) {
		qdf_mem_copy(&pAssocRsp->VHTCaps, &ar->VHTCaps,
			     sizeof(tDot11fIEVHTCaps));
		pe_debug("Received Assoc Response with VHT Cap");
		lim_log_vht_cap(mac, &pAssocRsp->VHTCaps);
	}
	if (ar->VHTOperation.present) {
		qdf_mem_copy(&pAssocRsp->VHTOperation, &ar->VHTOperation,
			     sizeof(tDot11fIEVHTOperation));
		pe_debug("Received Assoc Response with VHT Operation");
		lim_log_vht_operation(mac, &pAssocRsp->VHTOperation);
	}

	if (ar->ExtCap.present) {
		struct s_ext_cap *ext_cap;

		qdf_mem_copy(&pAssocRsp->ExtCap, &ar->ExtCap,
				sizeof(tDot11fIEExtCap));
		ext_cap = (struct s_ext_cap *)&pAssocRsp->ExtCap.bytes;
		pe_debug("timingMeas: %d, finetimingMeas Init: %d, Resp: %d",
			ext_cap->timing_meas, ext_cap->fine_time_meas_initiator,
			ext_cap->fine_time_meas_responder);
	}

	if (ar->QosMapSet.present) {
		pAssocRsp->QosMapSet.present = 1;
		convert_qos_mapset_frame(mac, &pAssocRsp->QosMapSet,
					 &ar->QosMapSet);
		pe_debug("Received Assoc Response with Qos Map Set");
		lim_log_qos_map_set(mac, &pAssocRsp->QosMapSet);
	}

	pAssocRsp->vendor_vht_ie.present = ar->vendor_vht_ie.present;
	if (ar->vendor_vht_ie.present)
		pAssocRsp->vendor_vht_ie.sub_type = ar->vendor_vht_ie.sub_type;
	if (ar->OBSSScanParameters.present) {
		qdf_mem_copy(&pAssocRsp->obss_scanparams,
				&ar->OBSSScanParameters,
				sizeof(struct sDot11fIEOBSSScanParameters));
	}
	if (ar->vendor_vht_ie.VHTCaps.present) {
		qdf_mem_copy(&pAssocRsp->vendor_vht_ie.VHTCaps,
				&ar->vendor_vht_ie.VHTCaps,
				sizeof(tDot11fIEVHTCaps));
		pe_debug("Received Assoc Response with Vendor specific VHT Cap");
		lim_log_vht_cap(mac, &pAssocRsp->VHTCaps);
	}
	if (ar->vendor_vht_ie.VHTOperation.present) {
		qdf_mem_copy(&pAssocRsp->vendor_vht_ie.VHTOperation,
				&ar->vendor_vht_ie.VHTOperation,
				sizeof(tDot11fIEVHTOperation));
		pe_debug("Received Assoc Response with Vendor specific VHT Oper");
		lim_log_vht_operation(mac, &pAssocRsp->VHTOperation);
	}

	if (ar->qcn_ie.present)
		qdf_mem_copy(&pAssocRsp->qcn_ie, &ar->qcn_ie,
			     sizeof(tDot11fIEqcn_ie));
	if (ar->he_cap.present) {
		qdf_mem_copy(&pAssocRsp->he_cap, &ar->he_cap,
			     sizeof(tDot11fIEhe_cap));
	}
	if (ar->he_op.present) {
		qdf_mem_copy(&pAssocRsp->he_op, &ar->he_op,
			     sizeof(tDot11fIEhe_op));
		pe_debug("bss_clr %d def_pe %d part_bss_clr %d bss_col_dis %d",
				pAssocRsp->he_op.bss_color,
				pAssocRsp->he_op.default_pe,
				pAssocRsp->he_op.partial_bss_col,
				pAssocRsp->he_op.bss_col_disabled);
	}

	if (ar->he_6ghz_band_cap.present) {
		pe_debug("11AX: HE Band Capability IE present");
		qdf_mem_copy(&pAssocRsp->he_6ghz_band_cap,
			     &ar->he_6ghz_band_cap,
			     sizeof(tDot11fIEhe_6ghz_band_cap));
	}

	if (ar->mu_edca_param_set.present) {
		pe_debug("11AX: HE MU EDCA param IE present");
		pAssocRsp->mu_edca_present = true;
		convert_mu_edca_param(mac, &pAssocRsp->mu_edca,
				&ar->mu_edca_param_set);
	}

	if (ar->MBO_IE.present && ar->MBO_IE.rssi_assoc_rej.present) {
		qdf_mem_copy(&pAssocRsp->rssi_assoc_rej,
				&ar->MBO_IE.rssi_assoc_rej,
				sizeof(tDot11fTLVrssi_assoc_rej));
		pe_debug("Received Assoc Response with rssi based assoc rej");
	}

	fils_convert_assoc_rsp_frame2_struct(ar, pAssocRsp);

	qdf_mem_free(ar);
	return QDF_STATUS_SUCCESS;

} /* End sir_convert_assoc_resp_frame2_struct. */

QDF_STATUS
sir_convert_reassoc_req_frame2_struct(struct mac_context *mac,
				      uint8_t *pFrame,
				      uint32_t nFrame, tpSirAssocReq pAssocReq)
{
	tDot11fReAssocRequest *ar;
	uint32_t status;

	ar = qdf_mem_malloc(sizeof(*ar));
	if (!ar)
		return QDF_STATUS_E_NOMEM;
	/* Zero-init our [out] parameter, */
	qdf_mem_zero((uint8_t *) pAssocReq, sizeof(tSirAssocReq));

	/* delegate to the framesc-generated code, */
	status = dot11f_unpack_re_assoc_request(mac, pFrame, nFrame,
						ar, false);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to parse a Re-association Request (0x%08x, %d bytes):",
			status, nFrame);
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_ERROR,
				   pFrame, nFrame);
		qdf_mem_free(ar);
		return QDF_STATUS_E_FAILURE;
	} else if (DOT11F_WARNED(status)) {
		pe_debug("There were warnings while unpacking a Re-association Request (0x%08x, %d bytes):",
			status, nFrame);
	}
	/* & "transliterate" from a 'tDot11fReAssocRequest' to a 'tSirAssocReq'... */

	/* make sure this is seen as a re-assoc request */
	pAssocReq->reassocRequest = 1;

	/* Capabilities */
	pAssocReq->capabilityInfo.ess = ar->Capabilities.ess;
	pAssocReq->capabilityInfo.ibss = ar->Capabilities.ibss;
	pAssocReq->capabilityInfo.cfPollable = ar->Capabilities.cfPollable;
	pAssocReq->capabilityInfo.cfPollReq = ar->Capabilities.cfPollReq;
	pAssocReq->capabilityInfo.privacy = ar->Capabilities.privacy;
	pAssocReq->capabilityInfo.shortPreamble = ar->Capabilities.shortPreamble;
	pAssocReq->capabilityInfo.pbcc = ar->Capabilities.pbcc;
	pAssocReq->capabilityInfo.channelAgility =
		ar->Capabilities.channelAgility;
	pAssocReq->capabilityInfo.spectrumMgt = ar->Capabilities.spectrumMgt;
	pAssocReq->capabilityInfo.qos = ar->Capabilities.qos;
	pAssocReq->capabilityInfo.shortSlotTime = ar->Capabilities.shortSlotTime;
	pAssocReq->capabilityInfo.apsd = ar->Capabilities.apsd;
	pAssocReq->capabilityInfo.rrm = ar->Capabilities.rrm;
	pAssocReq->capabilityInfo.dsssOfdm = ar->Capabilities.dsssOfdm;
	pAssocReq->capabilityInfo.delayedBA = ar->Capabilities.delayedBA;
	pAssocReq->capabilityInfo.immediateBA = ar->Capabilities.immediateBA;

	/* Listen Interval */
	pAssocReq->listenInterval = ar->ListenInterval.interval;

	/* SSID */
	if (ar->SSID.present) {
		pAssocReq->ssidPresent = 1;
		convert_ssid(mac, &pAssocReq->ssId, &ar->SSID);
	}
	/* Supported Rates */
	if (ar->SuppRates.present) {
		pAssocReq->suppRatesPresent = 1;
		convert_supp_rates(mac, &pAssocReq->supportedRates,
				   &ar->SuppRates);
	}
	/* Extended Supported Rates */
	if (ar->ExtSuppRates.present) {
		pAssocReq->extendedRatesPresent = 1;
		convert_ext_supp_rates(mac, &pAssocReq->extendedRates,
				       &ar->ExtSuppRates);
	}
	/* QOS Capabilities: */
	if (ar->QOSCapsStation.present) {
		pAssocReq->qosCapabilityPresent = 1;
		convert_qos_caps_station(mac, &pAssocReq->qosCapability,
					 &ar->QOSCapsStation);
	}
	/* WPA */
	if (ar->WPAOpaque.present) {
		pAssocReq->wpaPresent = 1;
		convert_wpa_opaque(mac, &pAssocReq->wpa, &ar->WPAOpaque);
	}
	/* RSN */
	if (ar->RSNOpaque.present) {
		pAssocReq->rsnPresent = 1;
		convert_rsn_opaque(mac, &pAssocReq->rsn, &ar->RSNOpaque);
	}

	/* Power Capabilities */
	if (ar->PowerCaps.present) {
		pAssocReq->powerCapabilityPresent = 1;
		convert_power_caps(mac, &pAssocReq->powerCapability,
				   &ar->PowerCaps);
	}
	/* Supported Channels */
	if (ar->SuppChannels.present) {
		pAssocReq->supportedChannelsPresent = 1;
		convert_supp_channels(mac, &pAssocReq->supportedChannels,
				      &ar->SuppChannels);
	}

	if (ar->HTCaps.present) {
		qdf_mem_copy(&pAssocReq->HTCaps, &ar->HTCaps,
			     sizeof(tDot11fIEHTCaps));
	}

	if (ar->WMMInfoStation.present) {
		pAssocReq->wmeInfoPresent = 1;
		qdf_mem_copy(&pAssocReq->WMMInfoStation, &ar->WMMInfoStation,
			     sizeof(tDot11fIEWMMInfoStation));

	}

	if (ar->WMMCaps.present)
		pAssocReq->wsmCapablePresent = 1;

	if (!pAssocReq->ssidPresent) {
		pe_debug("Received Assoc without SSID IE");
		qdf_mem_free(ar);
		return QDF_STATUS_E_FAILURE;
	}

	if (!pAssocReq->suppRatesPresent && !pAssocReq->extendedRatesPresent) {
		pe_debug("Received Assoc without supp rate IE");
		qdf_mem_free(ar);
		return QDF_STATUS_E_FAILURE;
	}
	/* Why no call to 'updateAssocReqFromPropCapability' here, like */
	/* there is in 'sir_convert_assoc_req_frame2_struct'? */

	/* WSC IE */
	if (ar->WscIEOpaque.present) {
		pAssocReq->addIEPresent = 1;
		convert_wsc_opaque(mac, &pAssocReq->addIE, &ar->WscIEOpaque);
	}

	if (ar->P2PIEOpaque.present) {
		pAssocReq->addIEPresent = 1;
		convert_p2p_opaque(mac, &pAssocReq->addIE, &ar->P2PIEOpaque);
	}
#ifdef WLAN_FEATURE_WFD
	if (ar->WFDIEOpaque.present) {
		pAssocReq->addIEPresent = 1;
		convert_wfd_opaque(mac, &pAssocReq->addIE, &ar->WFDIEOpaque);
	}
#endif
	if (ar->SuppOperatingClasses.present) {
		uint8_t num_classes = ar->SuppOperatingClasses.num_classes;

		if (num_classes > sizeof(ar->SuppOperatingClasses.classes))
			num_classes =
				sizeof(ar->SuppOperatingClasses.classes);
		qdf_mem_copy(&pAssocReq->supp_operating_classes,
			     &ar->SuppOperatingClasses,
			     sizeof(tDot11fIESuppOperatingClasses));
		QDF_TRACE_HEX_DUMP(
			QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			ar->SuppOperatingClasses.classes, num_classes);
	}
	if (ar->VHTCaps.present) {
		qdf_mem_copy(&pAssocReq->VHTCaps, &ar->VHTCaps,
			     sizeof(tDot11fIEVHTCaps));
	}
	if (ar->OperatingMode.present) {
		qdf_mem_copy(&pAssocReq->operMode, &ar->OperatingMode,
			     sizeof(tDot11fIEOperatingMode));
		pe_warn("Received Assoc Req with Operating Mode IE");
		lim_log_operating_mode(mac, &pAssocReq->operMode);
	}
	if (ar->ExtCap.present) {
		struct s_ext_cap *ext_cap;

		qdf_mem_copy(&pAssocReq->ExtCap, &ar->ExtCap,
			     sizeof(tDot11fIEExtCap));
		ext_cap = (struct s_ext_cap *)&pAssocReq->ExtCap.bytes;
		pe_debug("timingMeas: %d, finetimingMeas Init: %d, Resp: %d",
			ext_cap->timing_meas, ext_cap->fine_time_meas_initiator,
			ext_cap->fine_time_meas_responder);
	}
	if (ar->he_cap.present) {
		qdf_mem_copy(&pAssocReq->he_cap, &ar->he_cap,
			     sizeof(tDot11fIEhe_cap));
	}
	if (ar->he_6ghz_band_cap.present) {
		qdf_mem_copy(&pAssocReq->he_6ghz_band_cap,
			     &ar->he_6ghz_band_cap,
			     sizeof(tDot11fIEhe_6ghz_band_cap));
	}

	qdf_mem_free(ar);

	return QDF_STATUS_SUCCESS;

} /* End sir_convert_reassoc_req_frame2_struct. */

#ifdef FEATURE_WLAN_ESE
QDF_STATUS
sir_beacon_ie_ese_bcn_report(struct mac_context *mac,
	uint8_t *pPayload, const uint32_t nPayload,
	uint8_t **outIeBuf, uint32_t *pOutIeLen)
{
	tDot11fBeaconIEs *pBies = NULL;
	uint32_t status = QDF_STATUS_SUCCESS;
	QDF_STATUS retStatus = QDF_STATUS_SUCCESS;
	tSirEseBcnReportMandatoryIe eseBcnReportMandatoryIe;

	/* To store how many bytes are required to be allocated
	   for Bcn report mandatory Ies */
	uint16_t numBytes = 0, freeBytes = 0;
	uint8_t *pos = NULL;

	/* Zero-init our [out] parameter, */
	qdf_mem_zero((uint8_t *) &eseBcnReportMandatoryIe,
		    sizeof(eseBcnReportMandatoryIe));
	pBies = qdf_mem_malloc(sizeof(tDot11fBeaconIEs));
	if (!pBies)
		return QDF_STATUS_E_NOMEM;
	qdf_mem_zero(pBies, sizeof(tDot11fBeaconIEs));
	/* delegate to the framesc-generated code, */
	status = dot11f_unpack_beacon_i_es(mac, pPayload, nPayload,
					   pBies, false);

	if (DOT11F_FAILED(status)) {
		pe_err("Failed to parse Beacon IEs (0x%08x, %d bytes):",
			status, nPayload);
		qdf_mem_free(pBies);
		return QDF_STATUS_E_FAILURE;
	} else if (DOT11F_WARNED(status)) {
		pe_debug("There were warnings while unpacking Beacon IEs (0x%08x, %d bytes):",
			status, nPayload);
	}
	/* & "transliterate" from a 'tDot11fBeaconIEs' to a 'eseBcnReportMandatoryIe'... */
	if (!pBies->SSID.present) {
		pe_debug("Mandatory IE SSID not present!");
	} else {
		eseBcnReportMandatoryIe.ssidPresent = 1;
		convert_ssid(mac, &eseBcnReportMandatoryIe.ssId, &pBies->SSID);
		/* 1 for EID, 1 for length and length bytes */
		numBytes += 1 + 1 + eseBcnReportMandatoryIe.ssId.length;
	}

	if (!pBies->SuppRates.present) {
		pe_debug_rl("Mandatory IE Supported Rates not present!");
	} else {
		eseBcnReportMandatoryIe.suppRatesPresent = 1;
		convert_supp_rates(mac, &eseBcnReportMandatoryIe.supportedRates,
				   &pBies->SuppRates);
		numBytes +=
			1 + 1 + eseBcnReportMandatoryIe.supportedRates.numRates;
	}

	if (pBies->FHParamSet.present) {
		eseBcnReportMandatoryIe.fhParamPresent = 1;
		convert_fh_params(mac, &eseBcnReportMandatoryIe.fhParamSet,
				  &pBies->FHParamSet);
		numBytes += 1 + 1 + WLAN_FH_PARAM_IE_MAX_LEN;
	}

	if (pBies->DSParams.present) {
		eseBcnReportMandatoryIe.dsParamsPresent = 1;
		eseBcnReportMandatoryIe.dsParamSet.channelNumber =
			pBies->DSParams.curr_channel;
		numBytes += 1 + 1 + WLAN_DS_PARAM_IE_MAX_LEN;
	}

	if (pBies->CFParams.present) {
		eseBcnReportMandatoryIe.cfPresent = 1;
		convert_cf_params(mac, &eseBcnReportMandatoryIe.cfParamSet,
				  &pBies->CFParams);
		numBytes += 1 + 1 + WLAN_CF_PARAM_IE_MAX_LEN;
	}

	if (pBies->TIM.present) {
		eseBcnReportMandatoryIe.timPresent = 1;
		eseBcnReportMandatoryIe.tim.dtimCount = pBies->TIM.dtim_count;
		eseBcnReportMandatoryIe.tim.dtimPeriod = pBies->TIM.dtim_period;
		eseBcnReportMandatoryIe.tim.bitmapControl = pBies->TIM.bmpctl;
		/* As per the ESE spec, May truncate and report first 4 octets only */
		numBytes += 1 + 1 + SIR_MAC_TIM_EID_MIN;
	}

	if (pBies->RRMEnabledCap.present) {
		eseBcnReportMandatoryIe.rrmPresent = 1;
		qdf_mem_copy(&eseBcnReportMandatoryIe.rmEnabledCapabilities,
			     &pBies->RRMEnabledCap,
			     sizeof(tDot11fIERRMEnabledCap));
		numBytes += 1 + 1 + WLAN_RM_CAPABILITY_IE_MAX_LEN;
	}

	*outIeBuf = qdf_mem_malloc(numBytes);
	if (!*outIeBuf) {
		qdf_mem_free(pBies);
		return QDF_STATUS_E_NOMEM;
	}
	pos = *outIeBuf;
	*pOutIeLen = numBytes;
	freeBytes = numBytes;

	/* Start filling the output Ie with Mandatory IE information */
	/* Fill SSID IE */
	if (eseBcnReportMandatoryIe.ssidPresent) {
		if (freeBytes < (1 + 1 + eseBcnReportMandatoryIe.ssId.length)) {
			pe_err("Insufficient memory to copy SSID");
			retStatus = QDF_STATUS_E_FAILURE;
			goto err_bcnrep;
		}
		*pos = WLAN_ELEMID_SSID;
		pos++;
		*pos = eseBcnReportMandatoryIe.ssId.length;
		pos++;
		qdf_mem_copy(pos,
			     (uint8_t *) eseBcnReportMandatoryIe.ssId.ssId,
			     eseBcnReportMandatoryIe.ssId.length);
		pos += eseBcnReportMandatoryIe.ssId.length;
		freeBytes -= (1 + 1 + eseBcnReportMandatoryIe.ssId.length);
	}

	/* Fill Supported Rates IE */
	if (eseBcnReportMandatoryIe.suppRatesPresent) {
		if (freeBytes <
		    (1 + 1 + eseBcnReportMandatoryIe.supportedRates.numRates)) {
			pe_err("Insufficient memory to copy Rates IE");
			retStatus = QDF_STATUS_E_FAILURE;
			goto err_bcnrep;
		}
		if (eseBcnReportMandatoryIe.supportedRates.numRates <=
			WLAN_SUPPORTED_RATES_IE_MAX_LEN) {
			*pos = WLAN_ELEMID_RATES;
			pos++;
			*pos = eseBcnReportMandatoryIe.supportedRates.numRates;
			pos++;
			qdf_mem_copy(pos,
			     (uint8_t *) eseBcnReportMandatoryIe.supportedRates.
			     rate,
			     eseBcnReportMandatoryIe.supportedRates.numRates);
			pos += eseBcnReportMandatoryIe.supportedRates.numRates;
			freeBytes -=
			(1 + 1 +
			 eseBcnReportMandatoryIe.supportedRates.numRates);
		}
	}

	/* Fill FH Parameter set IE */
	if (eseBcnReportMandatoryIe.fhParamPresent) {
		if (freeBytes < (1 + 1 + WLAN_FH_PARAM_IE_MAX_LEN)) {
			pe_err("Insufficient memory to copy FHIE");
			retStatus = QDF_STATUS_E_FAILURE;
			goto err_bcnrep;
		}
		*pos = WLAN_ELEMID_FHPARMS;
		pos++;
		*pos = WLAN_FH_PARAM_IE_MAX_LEN;
		pos++;
		qdf_mem_copy(pos,
			     (uint8_t *) &eseBcnReportMandatoryIe.fhParamSet,
			     WLAN_FH_PARAM_IE_MAX_LEN);
		pos += WLAN_FH_PARAM_IE_MAX_LEN;
		freeBytes -= (1 + 1 + WLAN_FH_PARAM_IE_MAX_LEN);
	}

	/* Fill DS Parameter set IE */
	if (eseBcnReportMandatoryIe.dsParamsPresent) {
		if (freeBytes < (1 + 1 + WLAN_DS_PARAM_IE_MAX_LEN)) {
			pe_err("Insufficient memory to copy DS IE");
			retStatus = QDF_STATUS_E_FAILURE;
			goto err_bcnrep;
		}
		*pos = WLAN_ELEMID_DSPARMS;
		pos++;
		*pos = WLAN_DS_PARAM_IE_MAX_LEN;
		pos++;
		*pos = eseBcnReportMandatoryIe.dsParamSet.channelNumber;
		pos += WLAN_DS_PARAM_IE_MAX_LEN;
		freeBytes -= (1 + 1 + WLAN_DS_PARAM_IE_MAX_LEN);
	}

	/* Fill CF Parameter set */
	if (eseBcnReportMandatoryIe.cfPresent) {
		if (freeBytes < (1 + 1 + WLAN_CF_PARAM_IE_MAX_LEN)) {
			pe_err("Insufficient memory to copy CF IE");
			retStatus = QDF_STATUS_E_FAILURE;
			goto err_bcnrep;
		}
		*pos = WLAN_ELEMID_CFPARMS;
		pos++;
		*pos = WLAN_CF_PARAM_IE_MAX_LEN;
		pos++;
		qdf_mem_copy(pos,
			     (uint8_t *) &eseBcnReportMandatoryIe.cfParamSet,
			     WLAN_CF_PARAM_IE_MAX_LEN);
		pos += WLAN_CF_PARAM_IE_MAX_LEN;
		freeBytes -= (1 + 1 + WLAN_CF_PARAM_IE_MAX_LEN);
	}

	/* Fill TIM IE */
	if (eseBcnReportMandatoryIe.timPresent) {
		if (freeBytes < (1 + 1 + SIR_MAC_TIM_EID_MIN)) {
			pe_err("Insufficient memory to copy TIM IE");
			retStatus = QDF_STATUS_E_FAILURE;
			goto err_bcnrep;
		}
		*pos = WLAN_ELEMID_TIM;
		pos++;
		*pos = SIR_MAC_TIM_EID_MIN;
		pos++;
		qdf_mem_copy(pos,
			     (uint8_t *) &eseBcnReportMandatoryIe.tim,
			     SIR_MAC_TIM_EID_MIN);
		pos += SIR_MAC_TIM_EID_MIN;
		freeBytes -= (1 + 1 + SIR_MAC_TIM_EID_MIN);
	}

	/* Fill RM Capability IE */
	if (eseBcnReportMandatoryIe.rrmPresent) {
		if (freeBytes < (1 + 1 + WLAN_RM_CAPABILITY_IE_MAX_LEN)) {
			pe_err("Insufficient memory to copy RRM IE");
			retStatus = QDF_STATUS_E_FAILURE;
			goto err_bcnrep;
		}
		*pos = WLAN_ELEMID_RRM;
		pos++;
		*pos = WLAN_RM_CAPABILITY_IE_MAX_LEN;
		pos++;
		qdf_mem_copy(pos,
			     (uint8_t *) &eseBcnReportMandatoryIe.
			     rmEnabledCapabilities,
			     WLAN_RM_CAPABILITY_IE_MAX_LEN);
		freeBytes -= (1 + 1 + WLAN_RM_CAPABILITY_IE_MAX_LEN);
	}

	if (freeBytes != 0) {
		pe_err("Mismatch in allocation and copying of IE in Bcn Rep");
		retStatus = QDF_STATUS_E_FAILURE;
	}

err_bcnrep:
	/* The message counter would not be incremented in case of
	 * returning failure and hence next time, this function gets
	 * called, it would be using the same msg ctr for a different
	 * BSS.So, it is good to clear the memory allocated for a BSS
	 * that is returning failure.On success, the caller would take
	 * care of freeing up the memory*/
	if (retStatus == QDF_STATUS_E_FAILURE) {
		qdf_mem_free(*outIeBuf);
		*outIeBuf = NULL;
	}

	qdf_mem_free(pBies);
	return retStatus;
}

#endif /* FEATURE_WLAN_ESE */

#ifdef WLAN_FEATURE_11AX_BSS_COLOR
static void update_bss_color_change_from_beacon_ies(tDot11fBeaconIEs *bcn_ies,
		tpSirProbeRespBeacon bcn_struct)
{
	if (bcn_ies->bss_color_change.present) {
		qdf_mem_copy(&bcn_struct->vendor_he_bss_color_change,
			     &bcn_ies->bss_color_change,
			     sizeof(tDot11fIEbss_color_change));
	}
}
#else
static inline void update_bss_color_change_from_beacon_ies(
		tDot11fBeaconIEs *bcn_ies,
		tpSirProbeRespBeacon bcn_struct)
{}
#endif

QDF_STATUS
sir_parse_beacon_ie(struct mac_context *mac,
		    tpSirProbeRespBeacon pBeaconStruct,
		    uint8_t *pPayload, uint32_t nPayload)
{
	tDot11fBeaconIEs *pBies;
	uint32_t status;

	/* Zero-init our [out] parameter, */
	qdf_mem_zero((uint8_t *) pBeaconStruct, sizeof(tSirProbeRespBeacon));

	pBies = qdf_mem_malloc(sizeof(tDot11fBeaconIEs));
	if (!pBies)
		return QDF_STATUS_E_NOMEM;
	qdf_mem_zero(pBies, sizeof(tDot11fBeaconIEs));
	/* delegate to the framesc-generated code, */
	status = dot11f_unpack_beacon_i_es(mac, pPayload, nPayload,
					   pBies, false);

	if (DOT11F_FAILED(status)) {
		pe_err("Failed to parse Beacon IEs (0x%08x, %d bytes):",
			status, nPayload);
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_ERROR,
				   pPayload, nPayload);
		qdf_mem_free(pBies);
		return QDF_STATUS_E_FAILURE;
	} else if (DOT11F_WARNED(status)) {
		pe_debug("warnings (0x%08x, %d bytes):", status, nPayload);
	}
	/* & "transliterate" from a 'tDot11fBeaconIEs' to a 'tSirProbeRespBeacon'... */
	if (!pBies->SSID.present) {
		pe_debug("Mandatory IE SSID not present!");
	} else {
		pBeaconStruct->ssidPresent = 1;
		convert_ssid(mac, &pBeaconStruct->ssId, &pBies->SSID);
	}

	if (!pBies->SuppRates.present) {
		pe_debug_rl("Mandatory IE Supported Rates not present!");
	} else {
		pBeaconStruct->suppRatesPresent = 1;
		convert_supp_rates(mac, &pBeaconStruct->supportedRates,
				   &pBies->SuppRates);
	}

	if (pBies->ExtSuppRates.present) {
		pBeaconStruct->extendedRatesPresent = 1;
		convert_ext_supp_rates(mac, &pBeaconStruct->extendedRates,
				       &pBies->ExtSuppRates);
	}

	if (pBies->CFParams.present) {
		pBeaconStruct->cfPresent = 1;
		convert_cf_params(mac, &pBeaconStruct->cfParamSet,
				  &pBies->CFParams);
	}

	if (pBies->TIM.present) {
		pBeaconStruct->timPresent = 1;
		convert_tim(mac, &pBeaconStruct->tim, &pBies->TIM);
	}

	if (pBies->Country.present) {
		pBeaconStruct->countryInfoPresent = 1;
		convert_country(mac, &pBeaconStruct->countryInfoParam,
				&pBies->Country);
	}
	/* 11h IEs */
	if (pBies->TPCReport.present) {
		pBeaconStruct->tpcReportPresent = 1;
		qdf_mem_copy(&pBeaconStruct->tpcReport,
			     &pBies->TPCReport, sizeof(tDot11fIETPCReport));
	}

	if (pBies->PowerConstraints.present) {
		pBeaconStruct->powerConstraintPresent = 1;
		qdf_mem_copy(&pBeaconStruct->localPowerConstraint,
			     &pBies->PowerConstraints,
			     sizeof(tDot11fIEPowerConstraints));
	}
#ifdef FEATURE_WLAN_ESE
	if (pBies->ESEVersion.present)
		pBeaconStruct->is_ese_ver_ie_present = 1;
	if (pBies->ESETxmitPower.present) {
		pBeaconStruct->eseTxPwr.present = 1;
		pBeaconStruct->eseTxPwr.power_limit =
			pBies->ESETxmitPower.power_limit;
	}
	if (pBies->QBSSLoad.present) {
		qdf_mem_copy(&pBeaconStruct->QBSSLoad, &pBies->QBSSLoad,
			     sizeof(tDot11fIEQBSSLoad));
	}
#endif

	if (pBies->EDCAParamSet.present) {
		pBeaconStruct->edcaPresent = 1;
		convert_edca_param(mac, &pBeaconStruct->edcaParams,
				   &pBies->EDCAParamSet);
	}
	/* QOS Capabilities: */
	if (pBies->QOSCapsAp.present) {
		pBeaconStruct->qosCapabilityPresent = 1;
		convert_qos_caps(mac, &pBeaconStruct->qosCapability,
				 &pBies->QOSCapsAp);
	}

	if (pBies->ChanSwitchAnn.present) {
		pBeaconStruct->channelSwitchPresent = 1;
		qdf_mem_copy(&pBeaconStruct->channelSwitchIE,
			     &pBies->ChanSwitchAnn,
			     sizeof(pBeaconStruct->channelSwitchIE));
	}

	if (pBies->SuppOperatingClasses.present) {
		pBeaconStruct->supp_operating_class_present = 1;
		qdf_mem_copy(&pBeaconStruct->supp_operating_classes,
			&pBies->SuppOperatingClasses,
			sizeof(tDot11fIESuppOperatingClasses));
	}

	if (pBies->ext_chan_switch_ann.present) {
		pBeaconStruct->ext_chan_switch_present = 1;
		qdf_mem_copy(&pBeaconStruct->ext_chan_switch,
			     &pBies->ext_chan_switch_ann,
			     sizeof(tDot11fIEext_chan_switch_ann));
	}

	if (pBies->sec_chan_offset_ele.present) {
		pBeaconStruct->sec_chan_offset_present = 1;
		qdf_mem_copy(&pBeaconStruct->sec_chan_offset,
			     &pBies->sec_chan_offset_ele,
			     sizeof(pBeaconStruct->sec_chan_offset));
	}

	if (pBies->Quiet.present) {
		pBeaconStruct->quietIEPresent = 1;
		qdf_mem_copy(&pBeaconStruct->quietIE, &pBies->Quiet,
			     sizeof(tDot11fIEQuiet));
	}

	if (pBies->HTCaps.present) {
		qdf_mem_copy(&pBeaconStruct->HTCaps, &pBies->HTCaps,
			     sizeof(tDot11fIEHTCaps));
	}

	if (pBies->HTInfo.present) {
		qdf_mem_copy(&pBeaconStruct->HTInfo, &pBies->HTInfo,
			     sizeof(tDot11fIEHTInfo));
	}

	if (pBies->he_op.oper_info_6g_present) {
		pBeaconStruct->chan_freq = wlan_reg_chan_band_to_freq(mac->pdev,
						pBies->he_op.oper_info_6g.info.primary_ch,
						BIT(REG_BAND_6G));
	} else if (pBies->DSParams.present) {
		pBeaconStruct->dsParamsPresent = 1;
		pBeaconStruct->chan_freq =
		    wlan_reg_legacy_chan_to_freq(mac->pdev,
						 pBies->DSParams.curr_channel);
	} else if (pBies->HTInfo.present) {
		pBeaconStruct->chan_freq =
		    wlan_reg_legacy_chan_to_freq(mac->pdev,
						 pBies->HTInfo.primaryChannel);
	}

	if (pBies->RSN.present) {
		pBeaconStruct->rsnPresent = 1;
		convert_rsn(mac, &pBeaconStruct->rsn, &pBies->RSN);
	}

	if (pBies->WPA.present) {
		pBeaconStruct->wpaPresent = 1;
		convert_wpa(mac, &pBeaconStruct->wpa, &pBies->WPA);
	}

	if (pBies->WMMParams.present) {
		pBeaconStruct->wmeEdcaPresent = 1;
		convert_wmm_params(mac, &pBeaconStruct->edcaParams,
				   &pBies->WMMParams);
	}

	if (pBies->WMMInfoAp.present) {
		pBeaconStruct->wmeInfoPresent = 1;
	}

	if (pBies->WMMCaps.present) {
		pBeaconStruct->wsmCapablePresent = 1;
	}

	if (pBies->ERPInfo.present) {
		pBeaconStruct->erpPresent = 1;
		convert_erp_info(mac, &pBeaconStruct->erpIEInfo,
				 &pBies->ERPInfo);
	}
	if (pBies->VHTCaps.present) {
		pBeaconStruct->VHTCaps.present = 1;
		qdf_mem_copy(&pBeaconStruct->VHTCaps, &pBies->VHTCaps,
			     sizeof(tDot11fIEVHTCaps));
	}
	if (pBies->VHTOperation.present) {
		pBeaconStruct->VHTOperation.present = 1;
		qdf_mem_copy(&pBeaconStruct->VHTOperation, &pBies->VHTOperation,
			     sizeof(tDot11fIEVHTOperation));
	}
	if (pBies->VHTExtBssLoad.present) {
		pBeaconStruct->VHTExtBssLoad.present = 1;
		qdf_mem_copy(&pBeaconStruct->VHTExtBssLoad,
			     &pBies->VHTExtBssLoad,
			     sizeof(tDot11fIEVHTExtBssLoad));
	}
	if (pBies->OperatingMode.present) {
		pBeaconStruct->OperatingMode.present = 1;
		qdf_mem_copy(&pBeaconStruct->OperatingMode,
			     &pBies->OperatingMode,
			     sizeof(tDot11fIEOperatingMode));
	}
	if (pBies->MobilityDomain.present) {
		pBeaconStruct->mdiePresent = 1;
		qdf_mem_copy(pBeaconStruct->mdie, &pBies->MobilityDomain.MDID,
			     SIR_MDIE_SIZE);
	}

	pBeaconStruct->Vendor1IEPresent = pBies->Vendor1IE.present;
	pBeaconStruct->Vendor3IEPresent = pBies->Vendor3IE.present;
	pBeaconStruct->vendor_vht_ie.present = pBies->vendor_vht_ie.present;
	if (pBies->vendor_vht_ie.present)
		pBeaconStruct->vendor_vht_ie.sub_type =
						pBies->vendor_vht_ie.sub_type;

	if (pBies->vendor_vht_ie.VHTCaps.present) {
		pBeaconStruct->vendor_vht_ie.VHTCaps.present = 1;
		qdf_mem_copy(&pBeaconStruct->vendor_vht_ie.VHTCaps,
				&pBies->vendor_vht_ie.VHTCaps,
				sizeof(tDot11fIEVHTCaps));
	}
	if (pBies->vendor_vht_ie.VHTOperation.present) {
		pBeaconStruct->vendor_vht_ie.VHTOperation.present = 1;
		qdf_mem_copy(&pBeaconStruct->vendor_vht_ie.VHTOperation,
				&pBies->vendor_vht_ie.VHTOperation,
				sizeof(tDot11fIEVHTOperation));
	}
	if (pBies->ExtCap.present) {
		qdf_mem_copy(&pBeaconStruct->ext_cap, &pBies->ExtCap,
				sizeof(tDot11fIEExtCap));
	}
	/* Update HS 2.0 Information Element */
	if (pBies->hs20vendor_ie.present) {
		pe_debug("HS20 Indication Element Present, rel#:%u, id:%u",
			pBies->hs20vendor_ie.release_num,
			pBies->hs20vendor_ie.hs_id_present);
		qdf_mem_copy(&pBeaconStruct->hs20vendor_ie,
			&pBies->hs20vendor_ie,
			sizeof(tDot11fIEhs20vendor_ie) -
			sizeof(pBies->hs20vendor_ie.hs_id));
		if (pBies->hs20vendor_ie.hs_id_present)
			qdf_mem_copy(&pBeaconStruct->hs20vendor_ie.hs_id,
				&pBies->hs20vendor_ie.hs_id,
				sizeof(pBies->hs20vendor_ie.hs_id));
	}

	if (pBies->MBO_IE.present) {
		pBeaconStruct->MBO_IE_present = true;
		if (pBies->MBO_IE.cellular_data_cap.present)
			pBeaconStruct->MBO_capability =
				pBies->MBO_IE.cellular_data_cap.cellular_connectivity;

		if (pBies->MBO_IE.assoc_disallowed.present) {
			pBeaconStruct->assoc_disallowed = true;
			pBeaconStruct->assoc_disallowed_reason =
				pBies->MBO_IE.assoc_disallowed.reason_code;
		}
	}

	if (pBies->qcn_ie.present)
		qdf_mem_copy(&pBeaconStruct->qcn_ie, &pBies->qcn_ie,
			     sizeof(tDot11fIEqcn_ie));

	if (pBies->he_cap.present) {
		qdf_mem_copy(&pBeaconStruct->he_cap, &pBies->he_cap,
			     sizeof(tDot11fIEhe_cap));
	}
	if (pBies->he_op.present) {
		qdf_mem_copy(&pBeaconStruct->he_op, &pBies->he_op,
			     sizeof(tDot11fIEhe_op));
	}

	update_bss_color_change_from_beacon_ies(pBies, pBeaconStruct);

	qdf_mem_free(pBies);
	return QDF_STATUS_SUCCESS;
} /* End sir_parse_beacon_ie. */

#ifdef WLAN_FEATURE_11AX_BSS_COLOR
static void convert_bcon_bss_color_change_ie(tDot11fBeacon *bcn_frm,
		tpSirProbeRespBeacon bcn_struct)
{
	if (bcn_frm->bss_color_change.present) {
		pe_debug("11AX: HE BSS color change present");
		qdf_mem_copy(&bcn_struct->vendor_he_bss_color_change,
			     &bcn_frm->bss_color_change,
			     sizeof(tDot11fIEbss_color_change));
	}
}
#else
static inline void convert_bcon_bss_color_change_ie(tDot11fBeacon *bcn_frm,
		tpSirProbeRespBeacon bcn_struct)
{}
#endif

QDF_STATUS
sir_convert_beacon_frame2_struct(struct mac_context *mac,
				 uint8_t *pFrame,
				 tpSirProbeRespBeacon pBeaconStruct)
{
	tDot11fBeacon *pBeacon;
	uint32_t status, nPayload;
	uint8_t *pPayload;
	tpSirMacMgmtHdr pHdr;

	pPayload = WMA_GET_RX_MPDU_DATA(pFrame);
	nPayload = WMA_GET_RX_PAYLOAD_LEN(pFrame);
	pHdr = WMA_GET_RX_MAC_HEADER(pFrame);

	/* Zero-init our [out] parameter, */
	qdf_mem_zero((uint8_t *) pBeaconStruct, sizeof(tSirProbeRespBeacon));

	pBeacon = qdf_mem_malloc_atomic(sizeof(tDot11fBeacon));
	if (!pBeacon)
		return QDF_STATUS_E_NOMEM;

	/* get the MAC address out of the BD, */
	qdf_mem_copy(pBeaconStruct->bssid, pHdr->sa, 6);

	/* delegate to the framesc-generated code, */
	status = dot11f_unpack_beacon(mac, pPayload, nPayload, pBeacon, false);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to parse Beacon IEs (0x%08x, %d bytes):",
			status, nPayload);
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
				   pPayload, nPayload);
		qdf_mem_free(pBeacon);
		return QDF_STATUS_E_FAILURE;
	}
	/* & "transliterate" from a 'tDot11fBeacon' to a 'tSirProbeRespBeacon'... */
	/* Timestamp */
	qdf_mem_copy((uint8_t *) pBeaconStruct->timeStamp,
		     (uint8_t *) &pBeacon->TimeStamp,
		     sizeof(tSirMacTimeStamp));

	/* Beacon Interval */
	pBeaconStruct->beaconInterval = pBeacon->BeaconInterval.interval;

	/* Capabilities */
	pBeaconStruct->capabilityInfo.ess = pBeacon->Capabilities.ess;
	pBeaconStruct->capabilityInfo.ibss = pBeacon->Capabilities.ibss;
	pBeaconStruct->capabilityInfo.cfPollable =
		pBeacon->Capabilities.cfPollable;
	pBeaconStruct->capabilityInfo.cfPollReq =
		pBeacon->Capabilities.cfPollReq;
	pBeaconStruct->capabilityInfo.privacy = pBeacon->Capabilities.privacy;
	pBeaconStruct->capabilityInfo.shortPreamble =
		pBeacon->Capabilities.shortPreamble;
	pBeaconStruct->capabilityInfo.pbcc = pBeacon->Capabilities.pbcc;
	pBeaconStruct->capabilityInfo.channelAgility =
		pBeacon->Capabilities.channelAgility;
	pBeaconStruct->capabilityInfo.spectrumMgt =
		pBeacon->Capabilities.spectrumMgt;
	pBeaconStruct->capabilityInfo.qos = pBeacon->Capabilities.qos;
	pBeaconStruct->capabilityInfo.shortSlotTime =
		pBeacon->Capabilities.shortSlotTime;
	pBeaconStruct->capabilityInfo.apsd = pBeacon->Capabilities.apsd;
	pBeaconStruct->capabilityInfo.rrm = pBeacon->Capabilities.rrm;
	pBeaconStruct->capabilityInfo.dsssOfdm = pBeacon->Capabilities.dsssOfdm;
	pBeaconStruct->capabilityInfo.delayedBA =
		pBeacon->Capabilities.delayedBA;
	pBeaconStruct->capabilityInfo.immediateBA =
		pBeacon->Capabilities.immediateBA;

	if (!pBeacon->SSID.present) {
		pe_debug("Mandatory IE SSID not present!");
	} else {
		pBeaconStruct->ssidPresent = 1;
		convert_ssid(mac, &pBeaconStruct->ssId, &pBeacon->SSID);
	}

	if (!pBeacon->SuppRates.present) {
		pe_debug_rl("Mandatory IE Supported Rates not present!");
	} else {
		pBeaconStruct->suppRatesPresent = 1;
		convert_supp_rates(mac, &pBeaconStruct->supportedRates,
				   &pBeacon->SuppRates);
	}

	if (pBeacon->ExtSuppRates.present) {
		pBeaconStruct->extendedRatesPresent = 1;
		convert_ext_supp_rates(mac, &pBeaconStruct->extendedRates,
				       &pBeacon->ExtSuppRates);
	}

	if (pBeacon->CFParams.present) {
		pBeaconStruct->cfPresent = 1;
		convert_cf_params(mac, &pBeaconStruct->cfParamSet,
				  &pBeacon->CFParams);
	}

	if (pBeacon->TIM.present) {
		pBeaconStruct->timPresent = 1;
		convert_tim(mac, &pBeaconStruct->tim, &pBeacon->TIM);
	}

	if (pBeacon->Country.present) {
		pBeaconStruct->countryInfoPresent = 1;
		convert_country(mac, &pBeaconStruct->countryInfoParam,
				&pBeacon->Country);
	}
	/* QOS Capabilities: */
	if (pBeacon->QOSCapsAp.present) {
		pBeaconStruct->qosCapabilityPresent = 1;
		convert_qos_caps(mac, &pBeaconStruct->qosCapability,
				 &pBeacon->QOSCapsAp);
	}

	if (pBeacon->EDCAParamSet.present) {
		pBeaconStruct->edcaPresent = 1;
		convert_edca_param(mac, &pBeaconStruct->edcaParams,
				   &pBeacon->EDCAParamSet);
	}

	if (pBeacon->ChanSwitchAnn.present) {
		pBeaconStruct->channelSwitchPresent = 1;
		qdf_mem_copy(&pBeaconStruct->channelSwitchIE,
			     &pBeacon->ChanSwitchAnn,
			     sizeof(pBeaconStruct->channelSwitchIE));
	}

	if (pBeacon->ext_chan_switch_ann.present) {
		pBeaconStruct->ext_chan_switch_present = 1;
		qdf_mem_copy(&pBeaconStruct->ext_chan_switch,
			     &pBeacon->ext_chan_switch_ann,
			     sizeof(tDot11fIEext_chan_switch_ann));
	}

	if (pBeacon->sec_chan_offset_ele.present) {
		pBeaconStruct->sec_chan_offset_present = 1;
		qdf_mem_copy(&pBeaconStruct->sec_chan_offset,
			     &pBeacon->sec_chan_offset_ele,
			     sizeof(pBeaconStruct->sec_chan_offset));
	}

	if (pBeacon->TPCReport.present) {
		pBeaconStruct->tpcReportPresent = 1;
		qdf_mem_copy(&pBeaconStruct->tpcReport, &pBeacon->TPCReport,
			     sizeof(tDot11fIETPCReport));
	}

	if (pBeacon->PowerConstraints.present) {
		pBeaconStruct->powerConstraintPresent = 1;
		qdf_mem_copy(&pBeaconStruct->localPowerConstraint,
			     &pBeacon->PowerConstraints,
			     sizeof(tDot11fIEPowerConstraints));
	}

	if (pBeacon->Quiet.present) {
		pBeaconStruct->quietIEPresent = 1;
		qdf_mem_copy(&pBeaconStruct->quietIE, &pBeacon->Quiet,
			     sizeof(tDot11fIEQuiet));
	}

	if (pBeacon->HTCaps.present) {
		qdf_mem_copy(&pBeaconStruct->HTCaps, &pBeacon->HTCaps,
			     sizeof(tDot11fIEHTCaps));
	}

	if (pBeacon->HTInfo.present) {
		qdf_mem_copy(&pBeaconStruct->HTInfo, &pBeacon->HTInfo,
			     sizeof(tDot11fIEHTInfo));

	}

	if (pBeacon->he_op.oper_info_6g_present) {
		pBeaconStruct->chan_freq = wlan_reg_chan_band_to_freq(mac->pdev,
						pBeacon->he_op.oper_info_6g.info.primary_ch,
						BIT(REG_BAND_6G));
		pBeaconStruct->ap_power_type =
				pBeacon->he_op.oper_info_6g.info.reg_info;
	} else if (pBeacon->DSParams.present) {
		pBeaconStruct->dsParamsPresent = 1;
		pBeaconStruct->chan_freq =
		    wlan_reg_legacy_chan_to_freq(mac->pdev,
						 pBeacon->DSParams.curr_channel);
	} else if (pBeacon->HTInfo.present) {
		pBeaconStruct->chan_freq =
		    wlan_reg_legacy_chan_to_freq(mac->pdev,
						 pBeacon->HTInfo.primaryChannel);
	} else {
		pBeaconStruct->chan_freq = WMA_GET_RX_FREQ(pFrame);
		pe_debug_rl("In Beacon No Channel info");
	}

	if (pBeacon->RSN.present) {
		pBeaconStruct->rsnPresent = 1;
		convert_rsn(mac, &pBeaconStruct->rsn, &pBeacon->RSN);
	}

	if (pBeacon->WPA.present) {
		pBeaconStruct->wpaPresent = 1;
		convert_wpa(mac, &pBeaconStruct->wpa, &pBeacon->WPA);
	}

	if (pBeacon->WMMParams.present) {
		pBeaconStruct->wmeEdcaPresent = 1;
		convert_wmm_params(mac, &pBeaconStruct->edcaParams,
				   &pBeacon->WMMParams);
	}

	if (pBeacon->WMMInfoAp.present) {
		pBeaconStruct->wmeInfoPresent = 1;
		pe_debug("WMM Info present in Beacon Frame!");
	}

	if (pBeacon->WMMCaps.present) {
		pBeaconStruct->wsmCapablePresent = 1;
	}

	if (pBeacon->ERPInfo.present) {
		pBeaconStruct->erpPresent = 1;
		convert_erp_info(mac, &pBeaconStruct->erpIEInfo,
				 &pBeacon->ERPInfo);
	}
	if (pBeacon->MobilityDomain.present) {
		/* MobilityDomain */
		pBeaconStruct->mdiePresent = 1;
		qdf_mem_copy((uint8_t *) &(pBeaconStruct->mdie[0]),
			     (uint8_t *) &(pBeacon->MobilityDomain.MDID),
			     sizeof(uint16_t));
		pBeaconStruct->mdie[2] =
			((pBeacon->MobilityDomain.overDSCap << 0) | (pBeacon->
								     MobilityDomain.
								     resourceReqCap
								     << 1));

	}

#ifdef FEATURE_WLAN_ESE
	if (pBeacon->ESEVersion.present)
		pBeaconStruct->is_ese_ver_ie_present = 1;
	if (pBeacon->ESETxmitPower.present) {
		/* copy ESE TPC info element */
		pBeaconStruct->eseTxPwr.present = 1;
		qdf_mem_copy(&pBeaconStruct->eseTxPwr,
			     &pBeacon->ESETxmitPower,
			     sizeof(tDot11fIEESETxmitPower));
	}
	if (pBeacon->QBSSLoad.present) {
		qdf_mem_copy(&pBeaconStruct->QBSSLoad,
			     &pBeacon->QBSSLoad, sizeof(tDot11fIEQBSSLoad));
	}
#endif
	if (pBeacon->VHTCaps.present) {
		qdf_mem_copy(&pBeaconStruct->VHTCaps, &pBeacon->VHTCaps,
			     sizeof(tDot11fIEVHTCaps));
	}
	if (pBeacon->VHTOperation.present) {
		qdf_mem_copy(&pBeaconStruct->VHTOperation,
			     &pBeacon->VHTOperation,
			     sizeof(tDot11fIEVHTOperation));
	}
	if (pBeacon->VHTExtBssLoad.present) {
		qdf_mem_copy(&pBeaconStruct->VHTExtBssLoad,
			     &pBeacon->VHTExtBssLoad,
			     sizeof(tDot11fIEVHTExtBssLoad));
	}
	if (pBeacon->OperatingMode.present) {
		qdf_mem_copy(&pBeaconStruct->OperatingMode,
			     &pBeacon->OperatingMode,
			     sizeof(tDot11fIEOperatingMode));
	}
	if (pBeacon->WiderBWChanSwitchAnn.present) {
		pBeaconStruct->WiderBWChanSwitchAnnPresent = 1;
		qdf_mem_copy(&pBeaconStruct->WiderBWChanSwitchAnn,
			     &pBeacon->WiderBWChanSwitchAnn,
			     sizeof(tDot11fIEWiderBWChanSwitchAnn));
	}

	pBeaconStruct->Vendor1IEPresent = pBeacon->Vendor1IE.present;
	pBeaconStruct->Vendor3IEPresent = pBeacon->Vendor3IE.present;

	pBeaconStruct->vendor_vht_ie.present = pBeacon->vendor_vht_ie.present;
	if (pBeacon->vendor_vht_ie.present) {
		pBeaconStruct->vendor_vht_ie.sub_type =
			pBeacon->vendor_vht_ie.sub_type;
	}

	if (pBeacon->vendor_vht_ie.VHTCaps.present) {
		qdf_mem_copy(&pBeaconStruct->vendor_vht_ie.VHTCaps,
				&pBeacon->vendor_vht_ie.VHTCaps,
				sizeof(tDot11fIEVHTCaps));
	}
	if (pBeacon->vendor_vht_ie.VHTOperation.present) {
		qdf_mem_copy(&pBeaconStruct->vendor_vht_ie.VHTOperation,
				&pBeacon->VHTOperation,
				sizeof(tDot11fIEVHTOperation));
	}
	/* Update HS 2.0 Information Element */
	if (pBeacon->hs20vendor_ie.present) {
		qdf_mem_copy(&pBeaconStruct->hs20vendor_ie,
			&pBeacon->hs20vendor_ie,
			sizeof(tDot11fIEhs20vendor_ie) -
			sizeof(pBeacon->hs20vendor_ie.hs_id));
		if (pBeacon->hs20vendor_ie.hs_id_present)
			qdf_mem_copy(&pBeaconStruct->hs20vendor_ie.hs_id,
				&pBeacon->hs20vendor_ie.hs_id,
				sizeof(pBeacon->hs20vendor_ie.hs_id));
	}
#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
	if (pBeacon->QComVendorIE.present) {
		pBeaconStruct->AvoidChannelIE.present =
			pBeacon->QComVendorIE.present;
		pBeaconStruct->AvoidChannelIE.type =
			pBeacon->QComVendorIE.type;
		pBeaconStruct->AvoidChannelIE.channel =
			pBeacon->QComVendorIE.channel;
	}
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */
	if (pBeacon->OBSSScanParameters.present) {
		qdf_mem_copy(&pBeaconStruct->obss_scanparams,
			&pBeacon->OBSSScanParameters,
			sizeof(struct sDot11fIEOBSSScanParameters));
	}
	if (pBeacon->MBO_IE.present) {
		pBeaconStruct->MBO_IE_present = true;
		if (pBeacon->MBO_IE.cellular_data_cap.present)
			pBeaconStruct->MBO_capability =
				pBeacon->MBO_IE.cellular_data_cap.cellular_connectivity;

		if (pBeacon->MBO_IE.assoc_disallowed.present) {
			pBeaconStruct->assoc_disallowed = true;
			pBeaconStruct->assoc_disallowed_reason =
				pBeacon->MBO_IE.assoc_disallowed.reason_code;
		}
	}

	if (pBeacon->qcn_ie.present)
		qdf_mem_copy(&pBeaconStruct->qcn_ie, &pBeacon->qcn_ie,
			     sizeof(tDot11fIEqcn_ie));

	if (pBeacon->he_cap.present) {
		qdf_mem_copy(&pBeaconStruct->he_cap,
			     &pBeacon->he_cap,
			     sizeof(tDot11fIEhe_cap));
	}
	if (pBeacon->he_op.present) {
		qdf_mem_copy(&pBeaconStruct->he_op,
			     &pBeacon->he_op,
			     sizeof(tDot11fIEhe_op));
	}

	pBeaconStruct->num_transmit_power_env = pBeacon->num_transmit_power_env;
	if (pBeacon->num_transmit_power_env) {
		qdf_mem_copy(pBeaconStruct->transmit_power_env,
			     pBeacon->transmit_power_env,
			     pBeacon->num_transmit_power_env *
			     sizeof(tDot11fIEtransmit_power_env));
	}

	convert_bcon_bss_color_change_ie(pBeacon, pBeaconStruct);

	qdf_mem_free(pBeacon);
	return QDF_STATUS_SUCCESS;

} /* End sir_convert_beacon_frame2_struct. */

#ifdef WLAN_FEATURE_FILS_SK

/* update_ftie_in_fils_conf() - API to update fils info from auth
 * response packet from AP
 * @auth: auth packet pointer received from AP
 * @auth_frame: data structure needs to be updated
 *
 * Return: None
 */
static void
update_ftie_in_fils_conf(tDot11fAuthentication *auth,
			 tpSirMacAuthFrameBody auth_frame)
{
	/**
	 * Copy the FTIE sent by the AP in the auth request frame.
	 * This is required for FT-FILS connection.
	 * This FTIE will be sent in Assoc request frame without
	 * any modification.
	 */
	if (auth->FTInfo.present) {
		pe_debug("FT-FILS: r0kh_len:%d r1kh_present:%d",
			 auth->FTInfo.R0KH_ID.num_PMK_R0_ID,
			 auth->FTInfo.R1KH_ID.present);

		auth_frame->ft_ie.present = 1;
		if (auth->FTInfo.R1KH_ID.present) {
			qdf_mem_copy(auth_frame->ft_ie.r1kh_id,
				     auth->FTInfo.R1KH_ID.PMK_R1_ID,
				     FT_R1KH_ID_LEN);
		}

		if (auth->FTInfo.R0KH_ID.present) {
			qdf_mem_copy(auth_frame->ft_ie.r0kh_id,
				     auth->FTInfo.R0KH_ID.PMK_R0_ID,
				     auth->FTInfo.R0KH_ID.num_PMK_R0_ID);
			auth_frame->ft_ie.r0kh_id_len =
					auth->FTInfo.R0KH_ID.num_PMK_R0_ID;
		}

		if (auth_frame->ft_ie.gtk_ie.present) {
			pe_debug("FT-FILS: GTK present");
			qdf_mem_copy(&auth_frame->ft_ie.gtk_ie,
				     &auth->FTInfo.GTK,
				     sizeof(struct mac_ft_gtk_ie));
		}

		if (auth_frame->ft_ie.igtk_ie.present) {
			pe_debug("FT-FILS: IGTK present");
			qdf_mem_copy(&auth_frame->ft_ie.igtk_ie,
				     &auth->FTInfo.IGTK,
				     sizeof(struct mac_ft_igtk_ie));
		}

		qdf_mem_copy(auth_frame->ft_ie.anonce, auth->FTInfo.Anonce,
			     FT_NONCE_LEN);
		qdf_mem_copy(auth_frame->ft_ie.snonce, auth->FTInfo.Snonce,
			     FT_NONCE_LEN);

		qdf_mem_copy(auth_frame->ft_ie.mic, auth->FTInfo.MIC,
			     FT_MIC_LEN);
		auth_frame->ft_ie.element_count = auth->FTInfo.IECount;
	}
}

/* sir_update_auth_frame2_struct_fils_conf: API to update fils info from auth
 * packet type 2
 * @auth: auth packet pointer received from AP
 * @auth_frame: data structure needs to be updated
 *
 * Return: None
 */
static void
sir_update_auth_frame2_struct_fils_conf(tDot11fAuthentication *auth,
					tpSirMacAuthFrameBody auth_frame)
{
	if (auth->AuthAlgo.algo != SIR_FILS_SK_WITHOUT_PFS)
		return;

	if (auth->fils_assoc_delay_info.present)
		auth_frame->assoc_delay_info =
			auth->fils_assoc_delay_info.assoc_delay_info;

	if (auth->fils_session.present)
		qdf_mem_copy(auth_frame->session, auth->fils_session.session,
			SIR_FILS_SESSION_LENGTH);

	if (auth->fils_nonce.present)
		qdf_mem_copy(auth_frame->nonce, auth->fils_nonce.nonce,
			SIR_FILS_NONCE_LENGTH);

	if (auth->fils_wrapped_data.present) {
		qdf_mem_copy(auth_frame->wrapped_data,
			auth->fils_wrapped_data.wrapped_data,
			auth->fils_wrapped_data.num_wrapped_data);
		auth_frame->wrapped_data_len =
			auth->fils_wrapped_data.num_wrapped_data;
	}
	if (auth->RSNOpaque.present) {
		qdf_mem_copy(auth_frame->rsn_ie.info, auth->RSNOpaque.data,
			auth->RSNOpaque.num_data);
		auth_frame->rsn_ie.length = auth->RSNOpaque.num_data;
	}

	update_ftie_in_fils_conf(auth, auth_frame);

}
#else
static void sir_update_auth_frame2_struct_fils_conf(tDot11fAuthentication *auth,
				tpSirMacAuthFrameBody auth_frame)
{ }
#endif

QDF_STATUS
sir_convert_auth_frame2_struct(struct mac_context *mac,
			       uint8_t *pFrame,
			       uint32_t nFrame, tpSirMacAuthFrameBody pAuth)
{
	static tDot11fAuthentication auth;
	uint32_t status;

	/* Zero-init our [out] parameter, */
	qdf_mem_zero((uint8_t *) pAuth, sizeof(tSirMacAuthFrameBody));

	/* delegate to the framesc-generated code, */
	status = dot11f_unpack_authentication(mac, pFrame, nFrame,
					      &auth, false);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to parse an Authentication frame (0x%08x, %d bytes):",
			status, nFrame);
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_ERROR,
				   pFrame, nFrame);
		return QDF_STATUS_E_FAILURE;
	} else if (DOT11F_WARNED(status)) {
		pe_debug("There were warnings while unpacking an Authentication frame (0x%08x, %d bytes):",
			status, nFrame);
	}
	/* & "transliterate" from a 'tDot11fAuthentication' to a 'tSirMacAuthFrameBody'... */
	pAuth->authAlgoNumber = auth.AuthAlgo.algo;
	pAuth->authTransactionSeqNumber = auth.AuthSeqNo.no;
	pAuth->authStatusCode = auth.Status.status;

	if (auth.ChallengeText.present) {
		pAuth->type = WLAN_ELEMID_CHALLENGE;
		pAuth->length = auth.ChallengeText.num_text;
		qdf_mem_copy(pAuth->challengeText, auth.ChallengeText.text,
			     auth.ChallengeText.num_text);
	}
	sir_update_auth_frame2_struct_fils_conf(&auth, pAuth);

	return QDF_STATUS_SUCCESS;

} /* End sir_convert_auth_frame2_struct. */

QDF_STATUS
sir_convert_addts_rsp2_struct(struct mac_context *mac,
			      uint8_t *pFrame,
			      uint32_t nFrame, tSirAddtsRspInfo *pAddTs)
{
	tDot11fAddTSResponse addts = { {0} };
	tDot11fWMMAddTSResponse wmmaddts = { {0} };
	uint8_t j;
	uint16_t i;
	uint32_t status;

	if (QOS_ADD_TS_RSP != *(pFrame + 1)) {
		pe_err("Action of %d; this is not supported & is probably an error",
			*(pFrame + 1));
		return QDF_STATUS_E_FAILURE;
	}
	/* Zero-init our [out] parameter, */
	qdf_mem_zero((uint8_t *) pAddTs, sizeof(tSirAddtsRspInfo));
	qdf_mem_zero((uint8_t *) &addts, sizeof(tDot11fAddTSResponse));
	qdf_mem_zero((uint8_t *) &wmmaddts, sizeof(tDot11fWMMAddTSResponse));

	/* delegate to the framesc-generated code, */
	switch (*pFrame) {
	case ACTION_CATEGORY_QOS:
		status =
			dot11f_unpack_add_ts_response(mac, pFrame, nFrame,
						      &addts, false);
		break;
	case ACTION_CATEGORY_WMM:
		status =
			dot11f_unpack_wmm_add_ts_response(mac, pFrame, nFrame,
							  &wmmaddts, false);
		break;
	default:
		pe_err("Category of %d; this is not supported & is probably an error",
			*pFrame);
		return QDF_STATUS_E_FAILURE;
	}

	if (DOT11F_FAILED(status)) {
		pe_err("Failed to parse an Add TS Response frame (0x%08x, %d bytes):",
			status, nFrame);
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_ERROR,
				   pFrame, nFrame);
		return QDF_STATUS_E_FAILURE;
	} else if (DOT11F_WARNED(status)) {
		pe_debug("There were warnings while unpacking an Add TS Response frame (0x%08x,%d bytes):",
			status, nFrame);
	}
	/* & "transliterate" from a 'tDot11fAddTSResponse' or a */
	/* 'tDot11WMMAddTSResponse' to a 'tSirMacAddtsRspInfo'... */
	if (ACTION_CATEGORY_QOS == *pFrame) {
		pAddTs->dialogToken = addts.DialogToken.token;
		pAddTs->status = (enum wlan_status_code)addts.Status.status;

		if (addts.TSDelay.present) {
			convert_ts_delay(mac, &pAddTs->delay, &addts.TSDelay);
		}
		/* TS Delay is present iff status indicates its presence */
		if (pAddTs->status == STATUS_TS_NOT_CREATED &&
		    !addts.TSDelay.present) {
			pe_warn("Missing TSDelay IE");
		}

		if (addts.TSPEC.present) {
			convert_tspec(mac, &pAddTs->tspec, &addts.TSPEC);
		} else {
			pe_err("Mandatory TSPEC element missing in Add TS Response");
			return QDF_STATUS_E_FAILURE;
		}

		if (addts.num_TCLAS) {
			pAddTs->numTclas = (uint8_t) addts.num_TCLAS;

			for (i = 0U; i < addts.num_TCLAS; ++i) {
				if (QDF_STATUS_SUCCESS !=
				    convert_tclas(mac, &(pAddTs->tclasInfo[i]),
						  &(addts.TCLAS[i]))) {
					pe_err("Failed to convert a TCLAS IE");
					return QDF_STATUS_E_FAILURE;
				}
			}
		}

		if (addts.TCLASSPROC.present) {
			pAddTs->tclasProcPresent = 1;
			pAddTs->tclasProc = addts.TCLASSPROC.processing;
		}
#ifdef FEATURE_WLAN_ESE
		if (addts.ESETrafStrmMet.present) {
			pAddTs->tsmPresent = 1;
			qdf_mem_copy(&pAddTs->tsmIE.tsid,
				     &addts.ESETrafStrmMet.tsid,
				     sizeof(struct ese_tsm_ie));
		}
#endif
		if (addts.Schedule.present) {
			pAddTs->schedulePresent = 1;
			convert_schedule(mac, &pAddTs->schedule,
					 &addts.Schedule);
		}

		if (addts.WMMSchedule.present) {
			pAddTs->schedulePresent = 1;
			convert_wmm_schedule(mac, &pAddTs->schedule,
					     &addts.WMMSchedule);
		}

		if (addts.WMMTSPEC.present) {
			pAddTs->wsmTspecPresent = 1;
			convert_wmmtspec(mac, &pAddTs->tspec, &addts.WMMTSPEC);
		}

		if (addts.num_WMMTCLAS) {
			j = (uint8_t) (pAddTs->numTclas + addts.num_WMMTCLAS);
			if (SIR_MAC_TCLASIE_MAXNUM < j)
				j = SIR_MAC_TCLASIE_MAXNUM;

			for (i = pAddTs->numTclas; i < j; ++i) {
				if (QDF_STATUS_SUCCESS !=
				    convert_wmmtclas(mac,
						     &(pAddTs->tclasInfo[i]),
						     &(addts.WMMTCLAS[i]))) {
					pe_err("Failed to convert a TCLAS IE");
					return QDF_STATUS_E_FAILURE;
				}
			}
		}

		if (addts.WMMTCLASPROC.present) {
			pAddTs->tclasProcPresent = 1;
			pAddTs->tclasProc = addts.WMMTCLASPROC.processing;
		}

		if (1 < pAddTs->numTclas && (!pAddTs->tclasProcPresent)) {
			pe_err("%d TCLAS IE but not TCLASPROC IE",
				pAddTs->numTclas);
			return QDF_STATUS_E_FAILURE;
		}
	} else {
		pAddTs->dialogToken = wmmaddts.DialogToken.token;
		pAddTs->status =
			(enum wlan_status_code)wmmaddts.StatusCode.statusCode;

		if (wmmaddts.WMMTSPEC.present) {
			pAddTs->wmeTspecPresent = 1;
			convert_wmmtspec(mac, &pAddTs->tspec,
					 &wmmaddts.WMMTSPEC);
		} else {
			pe_err("Mandatory WME TSPEC element missing!");
			return QDF_STATUS_E_FAILURE;
		}

#ifdef FEATURE_WLAN_ESE
		if (wmmaddts.ESETrafStrmMet.present) {
			pAddTs->tsmPresent = 1;
			qdf_mem_copy(&pAddTs->tsmIE.tsid,
				     &wmmaddts.ESETrafStrmMet.tsid,
				     sizeof(struct ese_tsm_ie));
		}
#endif

	}

	return QDF_STATUS_SUCCESS;

} /* End sir_convert_addts_rsp2_struct. */

QDF_STATUS
sir_convert_delts_req2_struct(struct mac_context *mac,
			      uint8_t *pFrame,
			      uint32_t nFrame, struct delts_req_info *pDelTs)
{
	tDot11fDelTS delts = { {0} };
	tDot11fWMMDelTS wmmdelts = { {0} };
	uint32_t status;

	if (QOS_DEL_TS_REQ != *(pFrame + 1)) {
		pe_err("sirConvertDeltsRsp2Struct invoked "
			"with an Action of %d; this is not "
			"supported & is probably an error",
			*(pFrame + 1));
		return QDF_STATUS_E_FAILURE;
	}
	/* Zero-init our [out] parameter, */
	qdf_mem_zero(pDelTs, sizeof(*pDelTs));

	/* delegate to the framesc-generated code, */
	switch (*pFrame) {
	case ACTION_CATEGORY_QOS:
		status = dot11f_unpack_del_ts(mac, pFrame, nFrame,
					      &delts, false);
		break;
	case ACTION_CATEGORY_WMM:
		status = dot11f_unpack_wmm_del_ts(mac, pFrame, nFrame,
						  &wmmdelts, false);
		break;
	default:
		pe_err("sirConvertDeltsRsp2Struct invoked "
		       "with a Category of %d; this is not"
		       " supported & is probably an error",
			*pFrame);
		return QDF_STATUS_E_FAILURE;
	}

	if (DOT11F_FAILED(status)) {
		pe_err("Failed to parse an Del TS Request frame (0x%08x, %d bytes):",
			status, nFrame);
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_ERROR,
				   pFrame, nFrame);
		return QDF_STATUS_E_FAILURE;
	} else if (DOT11F_WARNED(status)) {
		pe_debug("There were warnings while unpacking an Del TS Request frame (0x%08x,%d bytes):",
			   status, nFrame);
	}
	/* & "transliterate" from a 'tDot11fDelTSResponse' or a */
	/* 'tDot11WMMDelTSResponse' to a 'tSirMacDeltsReqInfo'... */
	if (ACTION_CATEGORY_QOS == *pFrame) {
		pDelTs->tsinfo.traffic.trafficType =
			(uint16_t) delts.TSInfo.traffic_type;
		pDelTs->tsinfo.traffic.tsid = (uint16_t) delts.TSInfo.tsid;
		pDelTs->tsinfo.traffic.direction =
			(uint16_t) delts.TSInfo.direction;
		pDelTs->tsinfo.traffic.accessPolicy =
			(uint16_t) delts.TSInfo.access_policy;
		pDelTs->tsinfo.traffic.aggregation =
			(uint16_t) delts.TSInfo.aggregation;
		pDelTs->tsinfo.traffic.psb = (uint16_t) delts.TSInfo.psb;
		pDelTs->tsinfo.traffic.userPrio =
			(uint16_t) delts.TSInfo.user_priority;
		pDelTs->tsinfo.traffic.ackPolicy =
			(uint16_t) delts.TSInfo.tsinfo_ack_pol;

		pDelTs->tsinfo.schedule.schedule =
			(uint8_t) delts.TSInfo.schedule;
	} else {
		if (wmmdelts.WMMTSPEC.present) {
			pDelTs->wmeTspecPresent = 1;
			convert_wmmtspec(mac, &pDelTs->tspec,
					 &wmmdelts.WMMTSPEC);
		} else {
			pe_err("Mandatory WME TSPEC element missing!");
			return QDF_STATUS_E_FAILURE;
		}
	}

	return QDF_STATUS_SUCCESS;

} /* End sir_convert_delts_req2_struct. */

QDF_STATUS
sir_convert_qos_map_configure_frame2_struct(struct mac_context *mac,
					    uint8_t *pFrame,
					    uint32_t nFrame,
					    struct qos_map_set *pQosMapSet)
{
	tDot11fQosMapConfigure mapConfigure;
	uint32_t status;

	status =
		dot11f_unpack_qos_map_configure(mac, pFrame, nFrame,
						&mapConfigure, false);
	if (DOT11F_FAILED(status) || !mapConfigure.QosMapSet.present) {
		pe_err("Failed to parse Qos Map Configure frame (0x%08x, %d bytes):",
			   status, nFrame);
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_ERROR,
				   pFrame, nFrame);
		return QDF_STATUS_E_FAILURE;
	} else if (DOT11F_WARNED(status)) {
		pe_debug("There were warnings while unpacking Qos Map Configure frame (0x%08x, %d bytes):",
			   status, nFrame);
	}
	pQosMapSet->present = mapConfigure.QosMapSet.present;
	convert_qos_mapset_frame(mac, pQosMapSet, &mapConfigure.QosMapSet);
	lim_log_qos_map_set(mac, pQosMapSet);
	return QDF_STATUS_SUCCESS;
}

#ifdef ANI_SUPPORT_11H
QDF_STATUS
sir_convert_tpc_req_frame2_struct(struct mac_context *mac,
				  uint8_t *pFrame,
				  tpSirMacTpcReqActionFrame pTpcReqFrame,
				  uint32_t nFrame)
{
	tDot11fTPCRequest req;
	uint32_t status;

	qdf_mem_zero((uint8_t *) pTpcReqFrame,
			sizeof(tSirMacTpcReqActionFrame));
	status = dot11f_unpack_tpc_request(mac, pFrame, nFrame, &req, false);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to parse a TPC Request frame (0x%08x, %d bytes):",
			   status, nFrame);
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_ERROR,
				   pFrame, nFrame);
		return QDF_STATUS_E_FAILURE;
	} else if (DOT11F_WARNED(status)) {
		pe_debug("There were warnings while unpacking a TPC Request frame (0x%08x, %d bytes):",
			   status, nFrame);
	}
	/* & "transliterate" from a 'tDot11fTPCRequest' to a */
	/* 'tSirMacTpcReqActionFrame'... */
	pTpcReqFrame->actionHeader.category = req.Category.category;
	pTpcReqFrame->actionHeader.actionID = req.Action.action;
	pTpcReqFrame->actionHeader.dialogToken = req.DialogToken.token;
	if (req.TPCRequest.present) {
		pTpcReqFrame->type = DOT11F_EID_TPCREQUEST;
		pTpcReqFrame->length = 0;
	} else {
		pe_warn("!!!Rcv TPC Req of inalid type!");
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
} /* End sir_convert_tpc_req_frame2_struct. */
QDF_STATUS
sir_convert_meas_req_frame2_struct(struct mac_context *mac,
				   uint8_t *pFrame,
				   tpSirMacMeasReqActionFrame pMeasReqFrame,
				   uint32_t nFrame)
{
	tDot11fMeasurementRequest mr;
	uint32_t status;

	/* Zero-init our [out] parameter, */
	qdf_mem_zero((uint8_t *) pMeasReqFrame,
		    sizeof(tpSirMacMeasReqActionFrame));

	/* delegate to the framesc-generated code, */
	status = dot11f_unpack_measurement_request(mac, pFrame,
						   nFrame, &mr, false);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to parse a Measurement Request frame (0x%08x, %d bytes):",
			   status, nFrame);
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_ERROR,
				   pFrame, nFrame);
		return QDF_STATUS_E_FAILURE;
	} else if (DOT11F_WARNED(status)) {
		pe_debug("There were warnings while unpacking a Measurement Request frame (0x%08x, %d bytes):",
			   status, nFrame);
	}
	/* & "transliterate" from a 'tDot11fMeasurementRequest' to a */
	/* 'tpSirMacMeasReqActionFrame'... */
	pMeasReqFrame->actionHeader.category = mr.Category.category;
	pMeasReqFrame->actionHeader.actionID = mr.Action.action;
	pMeasReqFrame->actionHeader.dialogToken = mr.DialogToken.token;

	if (0 == mr.num_MeasurementRequest) {
		pe_err("Missing mandatory IE in Measurement Request Frame");
		return QDF_STATUS_E_FAILURE;
	} else if (1 < mr.num_MeasurementRequest) {
		pe_warn(
			FL("Warning: dropping extra Measurement Request IEs!"));
	}

	pMeasReqFrame->measReqIE.type = DOT11F_EID_MEASUREMENTREQUEST;
	pMeasReqFrame->measReqIE.length = DOT11F_IE_MEASUREMENTREQUEST_MIN_LEN;
	pMeasReqFrame->measReqIE.measToken =
		mr.MeasurementRequest[0].measurement_token;
	pMeasReqFrame->measReqIE.measReqMode =
		(mr.MeasurementRequest[0].reserved << 3) | (mr.
							    MeasurementRequest[0].
							    enable << 2) | (mr.
									    MeasurementRequest
									    [0].
									    request
									    << 1) |
		(mr.MeasurementRequest[0].report /*<< 0 */);
	pMeasReqFrame->measReqIE.measType =
		mr.MeasurementRequest[0].measurement_type;

	pMeasReqFrame->measReqIE.measReqField.channelNumber =
		mr.MeasurementRequest[0].channel_no;

	qdf_mem_copy(pMeasReqFrame->measReqIE.measReqField.measStartTime,
		     mr.MeasurementRequest[0].meas_start_time, 8);

	pMeasReqFrame->measReqIE.measReqField.measDuration =
		mr.MeasurementRequest[0].meas_duration;

	return QDF_STATUS_SUCCESS;

} /* End sir_convert_meas_req_frame2_struct. */
#endif

void populate_dot11f_tspec(struct mac_tspec_ie *pOld, tDot11fIETSPEC *pDot11f)
{
	pDot11f->traffic_type = pOld->tsinfo.traffic.trafficType;
	pDot11f->tsid = pOld->tsinfo.traffic.tsid;
	pDot11f->direction = pOld->tsinfo.traffic.direction;
	pDot11f->access_policy = pOld->tsinfo.traffic.accessPolicy;
	pDot11f->aggregation = pOld->tsinfo.traffic.aggregation;
	pDot11f->psb = pOld->tsinfo.traffic.psb;
	pDot11f->user_priority = pOld->tsinfo.traffic.userPrio;
	pDot11f->tsinfo_ack_pol = pOld->tsinfo.traffic.ackPolicy;
	pDot11f->schedule = pOld->tsinfo.schedule.schedule;
	/* As defined in IEEE 802.11-2007, section 7.3.2.30
	 * Nominal MSDU size: Bit[0:14]=Size, Bit[15]=Fixed
	 */
	pDot11f->size = (pOld->nomMsduSz & 0x7fff);
	pDot11f->fixed = (pOld->nomMsduSz & 0x8000) ? 1 : 0;
	pDot11f->max_msdu_size = pOld->maxMsduSz;
	pDot11f->min_service_int = pOld->minSvcInterval;
	pDot11f->max_service_int = pOld->maxSvcInterval;
	pDot11f->inactivity_int = pOld->inactInterval;
	pDot11f->suspension_int = pOld->suspendInterval;
	pDot11f->service_start_time = pOld->svcStartTime;
	pDot11f->min_data_rate = pOld->minDataRate;
	pDot11f->mean_data_rate = pOld->meanDataRate;
	pDot11f->peak_data_rate = pOld->peakDataRate;
	pDot11f->burst_size = pOld->maxBurstSz;
	pDot11f->delay_bound = pOld->delayBound;
	pDot11f->min_phy_rate = pOld->minPhyRate;
	pDot11f->surplus_bw_allowance = pOld->surplusBw;
	pDot11f->medium_time = pOld->mediumTime;

	pDot11f->present = 1;

} /* End populate_dot11f_tspec. */

#ifdef WLAN_FEATURE_MSCS
void
populate_dot11f_mscs_dec_element(struct mscs_req_info *mscs_req,
				 tDot11fmscs_request_action_frame *dot11f)
{
	dot11f->decriptor_element.request_type =
			mscs_req->dec.request_type;
	dot11f->decriptor_element.user_priority_control =
			mscs_req->dec.user_priority_control;
	dot11f->decriptor_element.stream_timeout =
			mscs_req->dec.stream_timeout;
	dot11f->decriptor_element.tclas_mask.classifier_type =
			mscs_req->dec.tclas_mask.classifier_type;
	dot11f->decriptor_element.tclas_mask.classifier_mask =
			mscs_req->dec.tclas_mask.classifier_mask;

	dot11f->decriptor_element.present = 1;
	dot11f->decriptor_element.tclas_mask.present = 1;

} /* End populate_dot11f_decriptor_element */
#endif

void populate_dot11f_wmmtspec(struct mac_tspec_ie *pOld,
			      tDot11fIEWMMTSPEC *pDot11f)
{
	pDot11f->traffic_type = pOld->tsinfo.traffic.trafficType;
	pDot11f->tsid = pOld->tsinfo.traffic.tsid;
	pDot11f->direction = pOld->tsinfo.traffic.direction;
	pDot11f->access_policy = pOld->tsinfo.traffic.accessPolicy;
	pDot11f->aggregation = pOld->tsinfo.traffic.aggregation;
	pDot11f->psb = pOld->tsinfo.traffic.psb;
	pDot11f->user_priority = pOld->tsinfo.traffic.userPrio;
	pDot11f->tsinfo_ack_pol = pOld->tsinfo.traffic.ackPolicy;
	pDot11f->burst_size_defn = pOld->tsinfo.traffic.burstSizeDefn;
	/* As defined in IEEE 802.11-2007, section 7.3.2.30
	 * Nominal MSDU size: Bit[0:14]=Size, Bit[15]=Fixed
	 */
	pDot11f->size = (pOld->nomMsduSz & 0x7fff);
	pDot11f->fixed = (pOld->nomMsduSz & 0x8000) ? 1 : 0;
	pDot11f->max_msdu_size = pOld->maxMsduSz;
	pDot11f->min_service_int = pOld->minSvcInterval;
	pDot11f->max_service_int = pOld->maxSvcInterval;
	pDot11f->inactivity_int = pOld->inactInterval;
	pDot11f->suspension_int = pOld->suspendInterval;
	pDot11f->service_start_time = pOld->svcStartTime;
	pDot11f->min_data_rate = pOld->minDataRate;
	pDot11f->mean_data_rate = pOld->meanDataRate;
	pDot11f->peak_data_rate = pOld->peakDataRate;
	pDot11f->burst_size = pOld->maxBurstSz;
	pDot11f->delay_bound = pOld->delayBound;
	pDot11f->min_phy_rate = pOld->minPhyRate;
	pDot11f->surplus_bw_allowance = pOld->surplusBw;
	pDot11f->medium_time = pOld->mediumTime;

	pDot11f->version = 1;
	pDot11f->present = 1;

} /* End populate_dot11f_wmmtspec. */

#if defined(FEATURE_WLAN_ESE)
/* Fill the ESE version currently supported */
void populate_dot11f_ese_version(tDot11fIEESEVersion *pESEVersion)
{
	pESEVersion->present = 1;
	pESEVersion->version = ESE_VERSION_SUPPORTED;
}

/* Fill the ESE ie for the station. */
/* The State is Normal (1) */
/* The MBSSID for station is set to 0. */
void populate_dot11f_ese_rad_mgmt_cap(tDot11fIEESERadMgmtCap *pESERadMgmtCap)
{
	pESERadMgmtCap->present = 1;
	pESERadMgmtCap->mgmt_state = RM_STATE_NORMAL;
	pESERadMgmtCap->mbssid_mask = 0;
	pESERadMgmtCap->reserved = 0;
}

QDF_STATUS populate_dot11f_ese_cckm_opaque(struct mac_context *mac,
					   tpSirCCKMie pCCKMie,
					   tDot11fIEESECckmOpaque *pDot11f)
{
	int idx;

	if (pCCKMie->length) {
		idx = find_ie_location(mac, (tpSirRSNie) pCCKMie,
						 DOT11F_EID_ESECCKMOPAQUE);
		if (0 <= idx) {
			pDot11f->present = 1;
			/* Dont include OUI */
			pDot11f->num_data = pCCKMie->cckmIEdata[idx + 1] - 4;
			qdf_mem_copy(pDot11f->data, pCCKMie->cckmIEdata + idx + 2 + 4,  /* EID,len,OUI */
				     pCCKMie->cckmIEdata[idx + 1] - 4); /* Skip OUI */
		}
	}
	return QDF_STATUS_SUCCESS;
} /* End populate_dot11f_ese_cckm_opaque. */

void populate_dot11_tsrsie(struct mac_context *mac,
			   struct ese_tsrs_ie *pOld,
			   tDot11fIEESETrafStrmRateSet *pDot11f,
			   uint8_t rate_length)
{
	pDot11f->tsid = pOld->tsid;
	qdf_mem_copy(pDot11f->tsrates, pOld->rates, rate_length);
	pDot11f->num_tsrates = rate_length;
	pDot11f->present = 1;
}
#endif

QDF_STATUS
populate_dot11f_tclas(struct mac_context *mac,
		      tSirTclasInfo *pOld, tDot11fIETCLAS *pDot11f)
{
	pDot11f->user_priority = pOld->tclas.userPrio;
	pDot11f->classifier_type = pOld->tclas.classifierType;
	pDot11f->classifier_mask = pOld->tclas.classifierMask;

	switch (pDot11f->classifier_type) {
	case SIR_MAC_TCLASTYPE_ETHERNET:
		qdf_mem_copy((uint8_t *) &pDot11f->info.EthParams.source,
			     (uint8_t *) &pOld->tclasParams.eth.srcAddr, 6);
		qdf_mem_copy((uint8_t *) &pDot11f->info.EthParams.dest,
			     (uint8_t *) &pOld->tclasParams.eth.dstAddr, 6);
		pDot11f->info.EthParams.type = pOld->tclasParams.eth.type;
		break;
	case SIR_MAC_TCLASTYPE_TCPUDPIP:
		pDot11f->info.IpParams.version = pOld->version;
		if (SIR_MAC_TCLAS_IPV4 == pDot11f->info.IpParams.version) {
			qdf_mem_copy(pDot11f->info.IpParams.params.IpV4Params.
				     source, pOld->tclasParams.ipv4.srcIpAddr,
				     4);
			qdf_mem_copy(pDot11f->info.IpParams.params.IpV4Params.
				     dest, pOld->tclasParams.ipv4.dstIpAddr, 4);
			pDot11f->info.IpParams.params.IpV4Params.src_port =
				pOld->tclasParams.ipv4.srcPort;
			pDot11f->info.IpParams.params.IpV4Params.dest_port =
				pOld->tclasParams.ipv4.dstPort;
			pDot11f->info.IpParams.params.IpV4Params.DSCP =
				pOld->tclasParams.ipv4.dscp;
			pDot11f->info.IpParams.params.IpV4Params.proto =
				pOld->tclasParams.ipv4.protocol;
			pDot11f->info.IpParams.params.IpV4Params.reserved =
				pOld->tclasParams.ipv4.rsvd;
		} else {
			qdf_mem_copy((uint8_t *) &pDot11f->info.IpParams.
				     params.IpV6Params.source,
				     (uint8_t *) pOld->tclasParams.ipv6.
				     srcIpAddr, 16);
			qdf_mem_copy((uint8_t *) &pDot11f->info.IpParams.
				     params.IpV6Params.dest,
				     (uint8_t *) pOld->tclasParams.ipv6.
				     dstIpAddr, 16);
			pDot11f->info.IpParams.params.IpV6Params.src_port =
				pOld->tclasParams.ipv6.srcPort;
			pDot11f->info.IpParams.params.IpV6Params.dest_port =
				pOld->tclasParams.ipv6.dstPort;
			qdf_mem_copy((uint8_t *) &pDot11f->info.IpParams.
				     params.IpV6Params.flow_label,
				     (uint8_t *) pOld->tclasParams.ipv6.
				     flowLabel, 3);
		}
		break;
	case SIR_MAC_TCLASTYPE_8021DQ:
		pDot11f->info.Params8021dq.tag_type =
			pOld->tclasParams.t8021dq.tag;
		break;
	default:
		pe_err("Bad TCLAS type %d", pDot11f->classifier_type);
		return QDF_STATUS_E_FAILURE;
	}

	pDot11f->present = 1;

	return QDF_STATUS_SUCCESS;

} /* End populate_dot11f_tclas. */

QDF_STATUS
populate_dot11f_wmmtclas(struct mac_context *mac,
			 tSirTclasInfo *pOld, tDot11fIEWMMTCLAS *pDot11f)
{
	pDot11f->version = 1;
	pDot11f->user_priority = pOld->tclas.userPrio;
	pDot11f->classifier_type = pOld->tclas.classifierType;
	pDot11f->classifier_mask = pOld->tclas.classifierMask;

	switch (pDot11f->classifier_type) {
	case SIR_MAC_TCLASTYPE_ETHERNET:
		qdf_mem_copy((uint8_t *) &pDot11f->info.EthParams.source,
			     (uint8_t *) &pOld->tclasParams.eth.srcAddr, 6);
		qdf_mem_copy((uint8_t *) &pDot11f->info.EthParams.dest,
			     (uint8_t *) &pOld->tclasParams.eth.dstAddr, 6);
		pDot11f->info.EthParams.type = pOld->tclasParams.eth.type;
		break;
	case SIR_MAC_TCLASTYPE_TCPUDPIP:
		pDot11f->info.IpParams.version = pOld->version;
		if (SIR_MAC_TCLAS_IPV4 == pDot11f->info.IpParams.version) {
			qdf_mem_copy((uint8_t *) &pDot11f->info.IpParams.
				     params.IpV4Params.source,
				     (uint8_t *) pOld->tclasParams.ipv4.
				     srcIpAddr, 4);
			qdf_mem_copy((uint8_t *) &pDot11f->info.IpParams.
				     params.IpV4Params.dest,
				     (uint8_t *) pOld->tclasParams.ipv4.
				     dstIpAddr, 4);
			pDot11f->info.IpParams.params.IpV4Params.src_port =
				pOld->tclasParams.ipv4.srcPort;
			pDot11f->info.IpParams.params.IpV4Params.dest_port =
				pOld->tclasParams.ipv4.dstPort;
			pDot11f->info.IpParams.params.IpV4Params.DSCP =
				pOld->tclasParams.ipv4.dscp;
			pDot11f->info.IpParams.params.IpV4Params.proto =
				pOld->tclasParams.ipv4.protocol;
			pDot11f->info.IpParams.params.IpV4Params.reserved =
				pOld->tclasParams.ipv4.rsvd;
		} else {
			qdf_mem_copy((uint8_t *) &pDot11f->info.IpParams.
				     params.IpV6Params.source,
				     (uint8_t *) pOld->tclasParams.ipv6.
				     srcIpAddr, 16);
			qdf_mem_copy((uint8_t *) &pDot11f->info.IpParams.
				     params.IpV6Params.dest,
				     (uint8_t *) pOld->tclasParams.ipv6.
				     dstIpAddr, 16);
			pDot11f->info.IpParams.params.IpV6Params.src_port =
				pOld->tclasParams.ipv6.srcPort;
			pDot11f->info.IpParams.params.IpV6Params.dest_port =
				pOld->tclasParams.ipv6.dstPort;
			qdf_mem_copy((uint8_t *) &pDot11f->info.IpParams.
				     params.IpV6Params.flow_label,
				     (uint8_t *) pOld->tclasParams.ipv6.
				     flowLabel, 3);
		}
		break;
	case SIR_MAC_TCLASTYPE_8021DQ:
		pDot11f->info.Params8021dq.tag_type =
			pOld->tclasParams.t8021dq.tag;
		break;
	default:
		pe_err("Bad TCLAS type %d in populate_dot11f_tclas",
			pDot11f->classifier_type);
		return QDF_STATUS_E_FAILURE;
	}

	pDot11f->present = 1;

	return QDF_STATUS_SUCCESS;

} /* End populate_dot11f_wmmtclas. */

QDF_STATUS populate_dot11f_wsc(struct mac_context *mac,
			       tDot11fIEWscBeacon *pDot11f)
{

	uint32_t wpsState;

	pDot11f->Version.present = 1;
	pDot11f->Version.major = 0x01;
	pDot11f->Version.minor = 0x00;

	wpsState = mac->mlme_cfg->wps_params.wps_state;

	pDot11f->WPSState.present = 1;
	pDot11f->WPSState.state = (uint8_t) wpsState;

	pDot11f->APSetupLocked.present = 0;

	pDot11f->SelectedRegistrar.present = 0;

	pDot11f->DevicePasswordID.present = 0;

	pDot11f->SelectedRegistrarConfigMethods.present = 0;

	pDot11f->UUID_E.present = 0;

	pDot11f->RFBands.present = 0;

	pDot11f->present = 1;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS populate_dot11f_wsc_registrar_info(struct mac_context *mac,
					      tDot11fIEWscBeacon *pDot11f)
{
	const struct sLimWscIeInfo *const pWscIeInfo = &(mac->lim.wscIeInfo);

	pDot11f->APSetupLocked.present = 1;
	pDot11f->APSetupLocked.fLocked = pWscIeInfo->apSetupLocked;

	pDot11f->SelectedRegistrar.present = 1;
	pDot11f->SelectedRegistrar.selected = pWscIeInfo->selectedRegistrar;

	pDot11f->DevicePasswordID.present = 1;
	pDot11f->DevicePasswordID.id =
		(uint16_t)mac->mlme_cfg->wps_params.wps_device_password_id;

	pDot11f->SelectedRegistrarConfigMethods.present = 1;
	pDot11f->SelectedRegistrarConfigMethods.methods =
		pWscIeInfo->selectedRegistrarConfigMethods;

	/* UUID_E and RF Bands are applicable only for dual band AP */

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS de_populate_dot11f_wsc_registrar_info(struct mac_context *mac,
						 tDot11fIEWscBeacon *pDot11f)
{
	pDot11f->APSetupLocked.present = 0;
	pDot11f->SelectedRegistrar.present = 0;
	pDot11f->DevicePasswordID.present = 0;
	pDot11f->SelectedRegistrarConfigMethods.present = 0;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS populate_dot11f_probe_res_wpsi_es(struct mac_context *mac,
					     tDot11fIEWscProbeRes *pDot11f,
					     struct pe_session *pe_session)
{

	tSirWPSProbeRspIE *pSirWPSProbeRspIE;

	pSirWPSProbeRspIE = &pe_session->APWPSIEs.SirWPSProbeRspIE;

	if (pSirWPSProbeRspIE->FieldPresent & SIR_WPS_PROBRSP_VER_PRESENT) {
		pDot11f->present = 1;
		pDot11f->Version.present = 1;
		pDot11f->Version.major =
			(uint8_t) ((pSirWPSProbeRspIE->Version & 0xF0) >> 4);
		pDot11f->Version.minor =
			(uint8_t) (pSirWPSProbeRspIE->Version & 0x0F);
	} else {
		pDot11f->present = 0;
		pDot11f->Version.present = 0;
	}

	if (pSirWPSProbeRspIE->FieldPresent & SIR_WPS_PROBRSP_STATE_PRESENT) {

		pDot11f->WPSState.present = 1;
		pDot11f->WPSState.state = (uint8_t) pSirWPSProbeRspIE->wpsState;
	} else
		pDot11f->WPSState.present = 0;

	if (pSirWPSProbeRspIE->
	    FieldPresent & SIR_WPS_PROBRSP_APSETUPLOCK_PRESENT) {
		pDot11f->APSetupLocked.present = 1;
		pDot11f->APSetupLocked.fLocked =
			pSirWPSProbeRspIE->APSetupLocked;
	} else
		pDot11f->APSetupLocked.present = 0;

	if (pSirWPSProbeRspIE->
	    FieldPresent & SIR_WPS_PROBRSP_SELECTEDREGISTRA_PRESENT) {
		pDot11f->SelectedRegistrar.present = 1;
		pDot11f->SelectedRegistrar.selected =
			pSirWPSProbeRspIE->SelectedRegistra;
	} else
		pDot11f->SelectedRegistrar.present = 0;

	if (pSirWPSProbeRspIE->
	    FieldPresent & SIR_WPS_PROBRSP_DEVICEPASSWORDID_PRESENT) {
		pDot11f->DevicePasswordID.present = 1;
		pDot11f->DevicePasswordID.id =
			pSirWPSProbeRspIE->DevicePasswordID;
	} else
		pDot11f->DevicePasswordID.present = 0;

	if (pSirWPSProbeRspIE->
	    FieldPresent & SIR_WPS_PROBRSP_SELECTEDREGISTRACFGMETHOD_PRESENT) {
		pDot11f->SelectedRegistrarConfigMethods.present = 1;
		pDot11f->SelectedRegistrarConfigMethods.methods =
			pSirWPSProbeRspIE->SelectedRegistraCfgMethod;
	} else
		pDot11f->SelectedRegistrarConfigMethods.present = 0;

	if (pSirWPSProbeRspIE->
	    FieldPresent & SIR_WPS_PROBRSP_RESPONSETYPE_PRESENT) {
		pDot11f->ResponseType.present = 1;
		pDot11f->ResponseType.resType = pSirWPSProbeRspIE->ResponseType;
	} else
		pDot11f->ResponseType.present = 0;

	if (pSirWPSProbeRspIE->FieldPresent & SIR_WPS_PROBRSP_UUIDE_PRESENT) {
		pDot11f->UUID_E.present = 1;
		qdf_mem_copy(pDot11f->UUID_E.uuid, pSirWPSProbeRspIE->UUID_E,
			     WNI_CFG_WPS_UUID_LEN);
	} else
		pDot11f->UUID_E.present = 0;

	if (pSirWPSProbeRspIE->
	    FieldPresent & SIR_WPS_PROBRSP_MANUFACTURE_PRESENT) {
		pDot11f->Manufacturer.present = 1;
		pDot11f->Manufacturer.num_name =
			pSirWPSProbeRspIE->Manufacture.num_name;
		qdf_mem_copy(pDot11f->Manufacturer.name,
			     pSirWPSProbeRspIE->Manufacture.name,
			     pSirWPSProbeRspIE->Manufacture.num_name);
	} else
		pDot11f->Manufacturer.present = 0;

	if (pSirWPSProbeRspIE->
	    FieldPresent & SIR_WPS_PROBRSP_MODELNUMBER_PRESENT) {
		pDot11f->ModelName.present = 1;
		pDot11f->ModelName.num_text =
			pSirWPSProbeRspIE->ModelName.num_text;
		qdf_mem_copy(pDot11f->ModelName.text,
			     pSirWPSProbeRspIE->ModelName.text,
			     pDot11f->ModelName.num_text);
	} else
		pDot11f->ModelName.present = 0;

	if (pSirWPSProbeRspIE->
	    FieldPresent & SIR_WPS_PROBRSP_MODELNUMBER_PRESENT) {
		pDot11f->ModelNumber.present = 1;
		pDot11f->ModelNumber.num_text =
			pSirWPSProbeRspIE->ModelNumber.num_text;
		qdf_mem_copy(pDot11f->ModelNumber.text,
			     pSirWPSProbeRspIE->ModelNumber.text,
			     pDot11f->ModelNumber.num_text);
	} else
		pDot11f->ModelNumber.present = 0;

	if (pSirWPSProbeRspIE->
	    FieldPresent & SIR_WPS_PROBRSP_SERIALNUMBER_PRESENT) {
		pDot11f->SerialNumber.present = 1;
		pDot11f->SerialNumber.num_text =
			pSirWPSProbeRspIE->SerialNumber.num_text;
		qdf_mem_copy(pDot11f->SerialNumber.text,
			     pSirWPSProbeRspIE->SerialNumber.text,
			     pDot11f->SerialNumber.num_text);
	} else
		pDot11f->SerialNumber.present = 0;

	if (pSirWPSProbeRspIE->
	    FieldPresent & SIR_WPS_PROBRSP_PRIMARYDEVICETYPE_PRESENT) {
		pDot11f->PrimaryDeviceType.present = 1;
		qdf_mem_copy(pDot11f->PrimaryDeviceType.oui,
			     pSirWPSProbeRspIE->PrimaryDeviceOUI,
			     sizeof(pSirWPSProbeRspIE->PrimaryDeviceOUI));
		pDot11f->PrimaryDeviceType.primary_category =
			(uint16_t) pSirWPSProbeRspIE->PrimaryDeviceCategory;
		pDot11f->PrimaryDeviceType.sub_category =
			(uint16_t) pSirWPSProbeRspIE->DeviceSubCategory;
	} else
		pDot11f->PrimaryDeviceType.present = 0;

	if (pSirWPSProbeRspIE->
	    FieldPresent & SIR_WPS_PROBRSP_DEVICENAME_PRESENT) {
		pDot11f->DeviceName.present = 1;
		pDot11f->DeviceName.num_text =
			pSirWPSProbeRspIE->DeviceName.num_text;
		qdf_mem_copy(pDot11f->DeviceName.text,
			     pSirWPSProbeRspIE->DeviceName.text,
			     pDot11f->DeviceName.num_text);
	} else
		pDot11f->DeviceName.present = 0;

	if (pSirWPSProbeRspIE->
	    FieldPresent & SIR_WPS_PROBRSP_CONFIGMETHODS_PRESENT) {
		pDot11f->ConfigMethods.present = 1;
		pDot11f->ConfigMethods.methods =
			pSirWPSProbeRspIE->ConfigMethod;
	} else
		pDot11f->ConfigMethods.present = 0;

	if (pSirWPSProbeRspIE->FieldPresent & SIR_WPS_PROBRSP_RF_BANDS_PRESENT) {
		pDot11f->RFBands.present = 1;
		pDot11f->RFBands.bands = pSirWPSProbeRspIE->RFBand;
	} else
		pDot11f->RFBands.present = 0;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS populate_dot11f_beacon_wpsi_es(struct mac_context *mac,
					  tDot11fIEWscBeacon *pDot11f,
					  struct pe_session *pe_session)
{

	tSirWPSBeaconIE *pSirWPSBeaconIE;

	pSirWPSBeaconIE = &pe_session->APWPSIEs.SirWPSBeaconIE;

	if (pSirWPSBeaconIE->FieldPresent & SIR_WPS_PROBRSP_VER_PRESENT) {
		pDot11f->present = 1;
		pDot11f->Version.present = 1;
		pDot11f->Version.major =
			(uint8_t) ((pSirWPSBeaconIE->Version & 0xF0) >> 4);
		pDot11f->Version.minor =
			(uint8_t) (pSirWPSBeaconIE->Version & 0x0F);
	} else {
		pDot11f->present = 0;
		pDot11f->Version.present = 0;
	}

	if (pSirWPSBeaconIE->FieldPresent & SIR_WPS_BEACON_STATE_PRESENT) {

		pDot11f->WPSState.present = 1;
		pDot11f->WPSState.state = (uint8_t) pSirWPSBeaconIE->wpsState;
	} else
		pDot11f->WPSState.present = 0;

	if (pSirWPSBeaconIE->FieldPresent & SIR_WPS_BEACON_APSETUPLOCK_PRESENT) {
		pDot11f->APSetupLocked.present = 1;
		pDot11f->APSetupLocked.fLocked = pSirWPSBeaconIE->APSetupLocked;
	} else
		pDot11f->APSetupLocked.present = 0;

	if (pSirWPSBeaconIE->
	    FieldPresent & SIR_WPS_BEACON_SELECTEDREGISTRA_PRESENT) {
		pDot11f->SelectedRegistrar.present = 1;
		pDot11f->SelectedRegistrar.selected =
			pSirWPSBeaconIE->SelectedRegistra;
	} else
		pDot11f->SelectedRegistrar.present = 0;

	if (pSirWPSBeaconIE->
	    FieldPresent & SIR_WPS_BEACON_DEVICEPASSWORDID_PRESENT) {
		pDot11f->DevicePasswordID.present = 1;
		pDot11f->DevicePasswordID.id =
			pSirWPSBeaconIE->DevicePasswordID;
	} else
		pDot11f->DevicePasswordID.present = 0;

	if (pSirWPSBeaconIE->
	    FieldPresent & SIR_WPS_BEACON_SELECTEDREGISTRACFGMETHOD_PRESENT) {
		pDot11f->SelectedRegistrarConfigMethods.present = 1;
		pDot11f->SelectedRegistrarConfigMethods.methods =
			pSirWPSBeaconIE->SelectedRegistraCfgMethod;
	} else
		pDot11f->SelectedRegistrarConfigMethods.present = 0;

	if (pSirWPSBeaconIE->FieldPresent & SIR_WPS_BEACON_UUIDE_PRESENT) {
		pDot11f->UUID_E.present = 1;
		qdf_mem_copy(pDot11f->UUID_E.uuid, pSirWPSBeaconIE->UUID_E,
			     WNI_CFG_WPS_UUID_LEN);
	} else
		pDot11f->UUID_E.present = 0;

	if (pSirWPSBeaconIE->FieldPresent & SIR_WPS_BEACON_RF_BANDS_PRESENT) {
		pDot11f->RFBands.present = 1;
		pDot11f->RFBands.bands = pSirWPSBeaconIE->RFBand;
	} else
		pDot11f->RFBands.present = 0;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS populate_dot11f_wsc_in_probe_res(struct mac_context *mac,
					    tDot11fIEWscProbeRes *pDot11f)
{
	uint32_t cfgStrLen;
	uint32_t val;
	uint32_t wpsVersion, wpsState;

	wpsVersion = mac->mlme_cfg->wps_params.wps_version;

	pDot11f->Version.present = 1;
	pDot11f->Version.major = (uint8_t) ((wpsVersion & 0xF0) >> 4);
	pDot11f->Version.minor = (uint8_t) (wpsVersion & 0x0F);

	wpsState = mac->mlme_cfg->wps_params.wps_state;
	pDot11f->WPSState.present = 1;
	pDot11f->WPSState.state = (uint8_t) wpsState;

	pDot11f->APSetupLocked.present = 0;

	pDot11f->SelectedRegistrar.present = 0;

	pDot11f->DevicePasswordID.present = 0;

	pDot11f->SelectedRegistrarConfigMethods.present = 0;

	pDot11f->ResponseType.present = 1;
	if ((mac->lim.wscIeInfo.reqType == REQ_TYPE_REGISTRAR) ||
	    (mac->lim.wscIeInfo.reqType == REQ_TYPE_WLAN_MANAGER_REGISTRAR)) {
		pDot11f->ResponseType.resType = RESP_TYPE_ENROLLEE_OPEN_8021X;
	} else {
		pDot11f->ResponseType.resType = RESP_TYPE_AP;
	}

	/* UUID is a 16 byte long binary*/
	pDot11f->UUID_E.present = 1;
	*pDot11f->UUID_E.uuid = '\0';

	wlan_mlme_get_wps_uuid(&mac->mlme_cfg->wps_params,
			       pDot11f->UUID_E.uuid);

	pDot11f->Manufacturer.present = 1;
	cfgStrLen = sizeof(pDot11f->Manufacturer.name);
	if (wlan_mlme_get_manufacturer_name(mac->psoc,
					    pDot11f->Manufacturer.name,
					    &cfgStrLen) != QDF_STATUS_SUCCESS) {
		pDot11f->Manufacturer.num_name = 0;
	} else {
		pDot11f->Manufacturer.num_name =
			(uint8_t) (cfgStrLen & 0x000000FF);
	}

	pDot11f->ModelName.present = 1;
	cfgStrLen = sizeof(pDot11f->ModelName.text);
	if (wlan_mlme_get_model_name(mac->psoc,
				     pDot11f->ModelName.text,
				     &cfgStrLen) != QDF_STATUS_SUCCESS) {
		pDot11f->ModelName.num_text = 0;
	} else {
		pDot11f->ModelName.num_text =
			(uint8_t) (cfgStrLen & 0x000000FF);
	}

	pDot11f->ModelNumber.present = 1;
	cfgStrLen = sizeof(pDot11f->ModelNumber.text);
	if (wlan_mlme_get_model_number(mac->psoc,
				       pDot11f->ModelNumber.text,
				       &cfgStrLen) != QDF_STATUS_SUCCESS) {
		pDot11f->ModelNumber.num_text = 0;
	} else {
		pDot11f->ModelNumber.num_text =
			(uint8_t) (cfgStrLen & 0x000000FF);
	}

	pDot11f->SerialNumber.present = 1;
	cfgStrLen = sizeof(pDot11f->SerialNumber.text);
	if (wlan_mlme_get_manufacture_product_version
				(mac->psoc,
				 pDot11f->SerialNumber.text,
				 &cfgStrLen) != QDF_STATUS_SUCCESS) {
		pDot11f->SerialNumber.num_text = 0;
	} else {
		pDot11f->SerialNumber.num_text =
			(uint8_t) (cfgStrLen & 0x000000FF);
	}

	pDot11f->PrimaryDeviceType.present = 1;

	pDot11f->PrimaryDeviceType.primary_category =
	       (uint16_t)mac->mlme_cfg->wps_params.wps_primary_device_category;

		val = mac->mlme_cfg->wps_params.wps_primary_device_oui;
		*(pDot11f->PrimaryDeviceType.oui) =
			(uint8_t) ((val >> 24) & 0xff);
		*(pDot11f->PrimaryDeviceType.oui + 1) =
			(uint8_t) ((val >> 16) & 0xff);
		*(pDot11f->PrimaryDeviceType.oui + 2) =
			(uint8_t) ((val >> 8) & 0xff);
		*(pDot11f->PrimaryDeviceType.oui + 3) =
			(uint8_t) ((val & 0xff));

	pDot11f->PrimaryDeviceType.sub_category =
	       (uint16_t)mac->mlme_cfg->wps_params.wps_device_sub_category;


	pDot11f->DeviceName.present = 1;
	cfgStrLen = sizeof(pDot11f->DeviceName.text);
	if (wlan_mlme_get_manufacture_product_name(mac->psoc,
						   pDot11f->DeviceName.text,
						   &cfgStrLen) !=
						   QDF_STATUS_SUCCESS) {
		pDot11f->DeviceName.num_text = 0;
	} else {
		pDot11f->DeviceName.num_text =
			(uint8_t) (cfgStrLen & 0x000000FF);
	}

		pDot11f->ConfigMethods.present = 1;
		pDot11f->ConfigMethods.methods =
			(uint16_t)(mac->mlme_cfg->wps_params.wps_cfg_method &
				   0x0000FFFF);

	pDot11f->RFBands.present = 0;

	pDot11f->present = 1;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
populate_dot11f_wsc_registrar_info_in_probe_res(struct mac_context *mac,
						tDot11fIEWscProbeRes *pDot11f)
{
	const struct sLimWscIeInfo *const pWscIeInfo = &(mac->lim.wscIeInfo);

	pDot11f->APSetupLocked.present = 1;
	pDot11f->APSetupLocked.fLocked = pWscIeInfo->apSetupLocked;

	pDot11f->SelectedRegistrar.present = 1;
	pDot11f->SelectedRegistrar.selected = pWscIeInfo->selectedRegistrar;

	pDot11f->DevicePasswordID.present = 1;
	pDot11f->DevicePasswordID.id =
		(uint16_t)mac->mlme_cfg->wps_params.wps_device_password_id;

	pDot11f->SelectedRegistrarConfigMethods.present = 1;
	pDot11f->SelectedRegistrarConfigMethods.methods =
		pWscIeInfo->selectedRegistrarConfigMethods;

	/* UUID_E and RF Bands are applicable only for dual band AP */

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
de_populate_dot11f_wsc_registrar_info_in_probe_res(struct mac_context *mac,
						   tDot11fIEWscProbeRes *
								 pDot11f)
{
	pDot11f->APSetupLocked.present = 0;
	pDot11f->SelectedRegistrar.present = 0;
	pDot11f->DevicePasswordID.present = 0;
	pDot11f->SelectedRegistrarConfigMethods.present = 0;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS populate_dot11_assoc_res_p2p_ie(struct mac_context *mac,
					   tDot11fIEP2PAssocRes *pDot11f,
					   tpSirAssocReq pRcvdAssocReq)
{
	const uint8_t *p2pIe;

	p2pIe = limGetP2pIEPtr(mac, pRcvdAssocReq->addIE.addIEdata,
			       pRcvdAssocReq->addIE.length);
	if (p2pIe) {
		pDot11f->present = 1;
		pDot11f->P2PStatus.present = 1;
		pDot11f->P2PStatus.status = QDF_STATUS_SUCCESS;
		pDot11f->ExtendedListenTiming.present = 0;
	}
	return QDF_STATUS_SUCCESS;
}


QDF_STATUS populate_dot11f_wfatpc(struct mac_context *mac,
				  tDot11fIEWFATPC *pDot11f, uint8_t txPower,
				  uint8_t linkMargin)
{
	pDot11f->txPower = txPower;
	pDot11f->linkMargin = linkMargin;
	pDot11f->present = 1;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
populate_dot11f_beacon_report(struct mac_context *mac,
			      tDot11fIEMeasurementReport *pDot11f,
			      tSirMacBeaconReport *pBeaconReport,
			      bool is_last_frame)
{
	tDot11fIEbeacon_report_frm_body_fragment_id *frm_body_frag_id;

	pDot11f->report.Beacon.regClass = pBeaconReport->regClass;
	pDot11f->report.Beacon.channel = pBeaconReport->channel;
	qdf_mem_copy(pDot11f->report.Beacon.meas_start_time,
		     pBeaconReport->measStartTime,
		     sizeof(pDot11f->report.Beacon.meas_start_time));
	pDot11f->report.Beacon.meas_duration = pBeaconReport->measDuration;
	pDot11f->report.Beacon.condensed_PHY = pBeaconReport->phyType;
	pDot11f->report.Beacon.reported_frame_type =
		!pBeaconReport->bcnProbeRsp;
	pDot11f->report.Beacon.RCPI = pBeaconReport->rcpi;
	pDot11f->report.Beacon.RSNI = pBeaconReport->rsni;
	qdf_mem_copy(pDot11f->report.Beacon.BSSID, pBeaconReport->bssid,
		     sizeof(tSirMacAddr));
	pDot11f->report.Beacon.antenna_id = pBeaconReport->antennaId;
	pDot11f->report.Beacon.parent_TSF = pBeaconReport->parentTSF;

	if (pBeaconReport->numIes) {
		pDot11f->report.Beacon.BeaconReportFrmBody.present = 1;
		qdf_mem_copy(pDot11f->report.Beacon.BeaconReportFrmBody.
			     reportedFields, pBeaconReport->Ies,
			     pBeaconReport->numIes);
		pDot11f->report.Beacon.BeaconReportFrmBody.num_reportedFields =
			pBeaconReport->numIes;
	}

	if (pBeaconReport->last_bcn_report_ind_support) {
		pe_debug("Including Last Beacon Report in RRM Frame");
		frm_body_frag_id = &pDot11f->report.Beacon.
			beacon_report_frm_body_fragment_id;

		frm_body_frag_id->present = 1;
		frm_body_frag_id->beacon_report_id =
			pBeaconReport->frame_body_frag_id.id;
		frm_body_frag_id->fragment_id_number =
			pBeaconReport->frame_body_frag_id.frag_id;
		frm_body_frag_id->more_fragments =
			pBeaconReport->frame_body_frag_id.more_frags;

		pDot11f->report.Beacon.last_beacon_report_indication.present =
			1;

		pDot11f->report.Beacon.last_beacon_report_indication.
			last_fragment = is_last_frame;
		pe_debug("id %d frag_id %d more_frags %d is_last_frame %d",
			 frm_body_frag_id->beacon_report_id,
			 frm_body_frag_id->fragment_id_number,
			 frm_body_frag_id->more_fragments,
			 is_last_frame);
	}
	return QDF_STATUS_SUCCESS;

}

QDF_STATUS populate_dot11f_rrm_ie(struct mac_context *mac,
				  tDot11fIERRMEnabledCap *pDot11f,
				  struct pe_session *pe_session)
{
	tpRRMCaps pRrmCaps;
	uint8_t *bytes;

	pRrmCaps = rrm_get_capabilities(mac, pe_session);

	pDot11f->LinkMeasurement = pRrmCaps->LinkMeasurement;
	pDot11f->NeighborRpt = pRrmCaps->NeighborRpt;
	pDot11f->parallel = pRrmCaps->parallel;
	pDot11f->repeated = pRrmCaps->repeated;
	pDot11f->BeaconPassive = pRrmCaps->BeaconPassive;
	pDot11f->BeaconActive = pRrmCaps->BeaconActive;
	pDot11f->BeaconTable = pRrmCaps->BeaconTable;
	pDot11f->BeaconRepCond = pRrmCaps->BeaconRepCond;
	pDot11f->FrameMeasurement = pRrmCaps->FrameMeasurement;
	pDot11f->ChannelLoad = pRrmCaps->ChannelLoad;
	pDot11f->NoiseHistogram = pRrmCaps->NoiseHistogram;
	pDot11f->statistics = pRrmCaps->statistics;
	pDot11f->LCIMeasurement = pRrmCaps->LCIMeasurement;
	pDot11f->LCIAzimuth = pRrmCaps->LCIAzimuth;
	pDot11f->TCMCapability = pRrmCaps->TCMCapability;
	pDot11f->triggeredTCM = pRrmCaps->triggeredTCM;
	pDot11f->APChanReport = pRrmCaps->APChanReport;
	pDot11f->RRMMIBEnabled = pRrmCaps->RRMMIBEnabled;
	pDot11f->operatingChanMax = pRrmCaps->operatingChanMax;
	pDot11f->nonOperatinChanMax = pRrmCaps->nonOperatingChanMax;
	pDot11f->MeasurementPilot = pRrmCaps->MeasurementPilot;
	pDot11f->MeasurementPilotEnabled = pRrmCaps->MeasurementPilotEnabled;
	pDot11f->NeighborTSFOffset = pRrmCaps->NeighborTSFOffset;
	pDot11f->RCPIMeasurement = pRrmCaps->RCPIMeasurement;
	pDot11f->RSNIMeasurement = pRrmCaps->RSNIMeasurement;
	pDot11f->BssAvgAccessDelay = pRrmCaps->BssAvgAccessDelay;
	pDot11f->BSSAvailAdmission = pRrmCaps->BSSAvailAdmission;
	pDot11f->AntennaInformation = pRrmCaps->AntennaInformation;
	pDot11f->fine_time_meas_rpt = pRrmCaps->fine_time_meas_rpt;
	pDot11f->lci_capability = pRrmCaps->lci_capability;
	pDot11f->reserved = pRrmCaps->reserved;

	bytes = (uint8_t *) pDot11f + 1; /* ignore present field */
	pe_debug("RRM Enabled Cap IE: %02x %02x %02x %02x %02x",
			   bytes[0], bytes[1], bytes[2], bytes[3], bytes[4]);

	pDot11f->present = 1;
	return QDF_STATUS_SUCCESS;
}

void populate_mdie(struct mac_context *mac,
		   tDot11fIEMobilityDomain *pDot11f,
		   uint8_t mdie[SIR_MDIE_SIZE])
{
	pDot11f->present = 1;
	pDot11f->MDID = (uint16_t) ((mdie[1] << 8) | (mdie[0]));

	/* Plugfest fix */
	pDot11f->overDSCap = (mdie[2] & 0x01);
	pDot11f->resourceReqCap = ((mdie[2] >> 1) & 0x01);

}

#ifdef WLAN_FEATURE_FILS_SK
void populate_fils_ft_info(struct mac_context *mac, tDot11fIEFTInfo *ft_info,
			   struct pe_session *pe_session)
{
	struct pe_fils_session *ft_fils_info = pe_session->fils_info;

	if (!ft_fils_info)
		return;

	if (!ft_fils_info->ft_ie.present) {
		ft_info->present = 0;
		pe_err("FT IE doesn't exist");
		return;
	}

	ft_info->IECount = ft_fils_info->ft_ie.element_count;

	qdf_mem_copy(ft_info->MIC, ft_fils_info->ft_ie.mic,
		     FT_MIC_LEN);

	qdf_mem_copy(ft_info->Anonce, ft_fils_info->ft_ie.anonce,
		     FT_NONCE_LEN);

	qdf_mem_copy(ft_info->Snonce, ft_fils_info->ft_ie.snonce,
		     FT_NONCE_LEN);

	if (ft_fils_info->ft_ie.r0kh_id_len > 0) {
		ft_info->R0KH_ID.present = 1;
		qdf_mem_copy(ft_info->R0KH_ID.PMK_R0_ID,
			     ft_fils_info->ft_ie.r0kh_id,
			     ft_fils_info->ft_ie.r0kh_id_len);
		ft_info->R0KH_ID.num_PMK_R0_ID =
				ft_fils_info->ft_ie.r0kh_id_len;
	}

	ft_info->R1KH_ID.present = 1;
	qdf_mem_copy(ft_info->R1KH_ID.PMK_R1_ID,
		     ft_fils_info->ft_ie.r1kh_id,
		     FT_R1KH_ID_LEN);

	qdf_mem_copy(&ft_info->GTK, &ft_fils_info->ft_ie.gtk_ie,
		     sizeof(struct mac_ft_gtk_ie));
	qdf_mem_copy(&ft_info->IGTK, &ft_fils_info->ft_ie.igtk_ie,
		     sizeof(struct mac_ft_igtk_ie));

	ft_info->present = 1;
}
#endif

void populate_dot11f_assoc_rsp_rates(struct mac_context *mac,
				     tDot11fIESuppRates *pSupp,
				     tDot11fIEExtSuppRates *pExt,
				     uint16_t *_11bRates, uint16_t *_11aRates)
{
	uint8_t num_supp = 0, num_ext = 0;
	uint8_t i, j;

	for (i = 0; (i < SIR_NUM_11B_RATES && _11bRates[i]); i++, num_supp++) {
		pSupp->rates[num_supp] = (uint8_t) _11bRates[i];
	}
	for (j = 0; (j < SIR_NUM_11A_RATES && _11aRates[j]); j++) {
		if (num_supp < 8)
			pSupp->rates[num_supp++] = (uint8_t) _11aRates[j];
		else
			pExt->rates[num_ext++] = (uint8_t) _11aRates[j];
	}

	if (num_supp) {
		pSupp->num_rates = num_supp;
		pSupp->present = 1;
	}
	if (num_ext) {
		pExt->num_rates = num_ext;
		pExt->present = 1;
	}
}

void populate_dot11f_timeout_interval(struct mac_context *mac,
				      tDot11fIETimeoutInterval *pDot11f,
				      uint8_t type, uint32_t value)
{
	pDot11f->present = 1;
	pDot11f->timeoutType = type;
	pDot11f->timeoutValue = value;
}

/**
 * populate_dot11f_timing_advert_frame() - Populate the TA mgmt frame fields
 * @mac: the MAC context
 * @frame: pointer to the TA frame
 *
 * Return: The SIR status.
 */
QDF_STATUS
populate_dot11f_timing_advert_frame(struct mac_context *mac_ctx,
				    tDot11fTimingAdvertisementFrame *frame)
{
	uint32_t val = 0;

	/* Capabilities */
	val = mac_ctx->mlme_cfg->wep_params.is_privacy_enabled;
	if (val)
		frame->Capabilities.privacy = 1;

	if (mac_ctx->mlme_cfg->ht_caps.short_preamble)
		frame->Capabilities.shortPreamble = 1;

	if (mac_ctx->mlme_cfg->gen.enabled_11h)
		frame->Capabilities.spectrumMgt = 1;

	if (mac_ctx->mlme_cfg->wmm_params.qos_enabled)
		frame->Capabilities.qos = 1;

	if (mac_ctx->mlme_cfg->roam_scoring.apsd_enabled)
		frame->Capabilities.apsd = 1;

	val = mac_ctx->mlme_cfg->feature_flags.enable_block_ack;
	frame->Capabilities.delayedBA =
		(uint16_t)((val >> WNI_CFG_BLOCK_ACK_ENABLED_DELAYED) & 1);
	frame->Capabilities.immediateBA =
		(uint16_t)((val >> WNI_CFG_BLOCK_ACK_ENABLED_IMMEDIATE) & 1);

	/* Country */
	populate_dot11f_country(mac_ctx, &frame->Country, NULL);

	/* PowerConstraints */
	frame->PowerConstraints.localPowerConstraints =
		mac_ctx->mlme_cfg->power.local_power_constraint;

	frame->PowerConstraints.present = 1;

	/* TimeAdvertisement */
	frame->TimeAdvertisement.present = 1;
	frame->TimeAdvertisement.timing_capabilities = 1;

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11AX
#ifdef WLAN_SUPPORT_TWT
static void
populate_dot11f_twt_he_cap(struct mac_context *mac,
			   struct pe_session *session,
			   tDot11fIEhe_cap *he_cap)
{
	bool bcast_requestor =
		mac->mlme_cfg->twt_cfg.is_bcast_requestor_enabled;
	bool bcast_responder =
		mac->mlme_cfg->twt_cfg.is_bcast_responder_enabled;

	he_cap->broadcast_twt = 0;
	if (session->opmode == QDF_STA_MODE &&
	    !(mac->mlme_cfg->twt_cfg.req_flag)) {
		/* Set twt_request as 0 if any SCC/MCC concurrency exist */
		he_cap->twt_request = 0;
		return;
	} else if (session->opmode == QDF_SAP_MODE &&
		   !(mac->mlme_cfg->twt_cfg.res_flag)) {
		/** Set twt_responder as 0 if any SCC/MCC concurrency exist */
		he_cap->twt_responder = 0;
		return;
	}

	if (session->opmode == QDF_STA_MODE) {
		he_cap->broadcast_twt = bcast_requestor;
	} else if (session->opmode == QDF_SAP_MODE) {
		he_cap->broadcast_twt = bcast_responder;
	}
}
#else
static inline void
populate_dot11f_twt_he_cap(struct mac_context *mac_ctx,
			   struct pe_session *session,
			   tDot11fIEhe_cap *he_cap)
{
	he_cap->broadcast_twt = 0;
}
#endif

/**
 * populate_dot11f_he_caps() - pouldate HE Capability IE
 * @mac_ctx: Global MAC context
 * @session: PE session
 * @he_cap: pointer to HE capability IE
 *
 * Populdate the HE capability IE based on the session.
 */
QDF_STATUS populate_dot11f_he_caps(struct mac_context *mac_ctx, struct pe_session *session,
				   tDot11fIEhe_cap *he_cap)
{
	uint8_t *ppet;
	uint32_t value = 0;

	he_cap->present = 1;

	if (!session) {
		qdf_mem_copy(he_cap, &mac_ctx->mlme_cfg->he_caps.dot11_he_cap,
			     sizeof(tDot11fIEhe_cap));
		return QDF_STATUS_SUCCESS;
	}

	/** TODO: String items needs attention. **/
	qdf_mem_copy(he_cap, &session->he_config, sizeof(*he_cap));
	if (he_cap->ppet_present) {
		value = WNI_CFG_HE_PPET_LEN;
		/* if session is present, populate PPET based on band */
		if (!wlan_reg_is_24ghz_ch_freq(session->curr_op_freq))
			qdf_mem_copy(he_cap->ppet.ppe_threshold.ppe_th,
				     mac_ctx->mlme_cfg->he_caps.he_ppet_5g,
				     value);
		else
			qdf_mem_copy(he_cap->ppet.ppe_threshold.ppe_th,
				     mac_ctx->mlme_cfg->he_caps.he_ppet_2g,
				     value);

		ppet = he_cap->ppet.ppe_threshold.ppe_th;
		he_cap->ppet.ppe_threshold.num_ppe_th =
						lim_truncate_ppet(ppet, value);
	} else {
		he_cap->ppet.ppe_threshold.num_ppe_th = 0;
	}
	populate_dot11f_twt_he_cap(mac_ctx, session, he_cap);

	return QDF_STATUS_SUCCESS;
}

/**
 * populate_dot11f_he_operation() - pouldate HE Operation IE
 * @mac_ctx: Global MAC context
 * @session: PE session
 * @he_op: pointer to HE Operation IE
 *
 * Populdate the HE Operation IE based on the session.
 */
QDF_STATUS
populate_dot11f_he_operation(struct mac_context *mac_ctx,
			     struct pe_session *session, tDot11fIEhe_op *he_op)
{
	enum reg_6g_ap_type ap_pwr_type;
	qdf_mem_copy(he_op, &session->he_op, sizeof(*he_op));

	he_op->present = 1;
	he_op->vht_oper_present = 0;
	if (session->he_6ghz_band) {
		he_op->oper_info_6g_present = 1;
		he_op->oper_info_6g.info.ch_width = session->ch_width;
		he_op->oper_info_6g.info.center_freq_seg0 =
					session->ch_center_freq_seg0;
		if (session->ch_width == CH_WIDTH_80P80MHZ ||
		    session->ch_width == CH_WIDTH_160MHZ) {
			he_op->oper_info_6g.info.center_freq_seg1 =
				session->ch_center_freq_seg1;
			he_op->oper_info_6g.info.ch_width = CH_WIDTH_160MHZ;
		} else {
			he_op->oper_info_6g.info.center_freq_seg1 = 0;
		}
		he_op->oper_info_6g.info.primary_ch =
			wlan_reg_freq_to_chan(mac_ctx->pdev,
					      session->curr_op_freq);
		he_op->oper_info_6g.info.dup_bcon = 0;
		he_op->oper_info_6g.info.min_rate = 0;
		wlan_reg_get_cur_6g_ap_pwr_type(mac_ctx->pdev, &ap_pwr_type);
		he_op->oper_info_6g.info.reg_info = ap_pwr_type;
	}
	lim_log_he_op(mac_ctx, he_op, session);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
populate_dot11f_he_6ghz_cap(struct mac_context *mac_ctx,
			    struct pe_session *session,
			    tDot11fIEhe_6ghz_band_cap *he_6g_cap)
{
	struct mlme_ht_capabilities_info *ht_cap_info;
	struct mlme_vht_capabilities_info *vht_cap_info;

	if (session && !session->he_6ghz_band)
		return QDF_STATUS_SUCCESS;

	ht_cap_info = &mac_ctx->mlme_cfg->ht_caps.ht_cap_info;
	vht_cap_info = &mac_ctx->mlme_cfg->vht_caps.vht_cap_info;

	he_6g_cap->present = 1;
	he_6g_cap->min_mpdu_start_spacing =
		mac_ctx->mlme_cfg->ht_caps.ampdu_params.mpdu_density;
	if (session)
		he_6g_cap->max_ampdu_len_exp =
			session->vht_config.max_ampdu_lenexp;
	else
		he_6g_cap->max_ampdu_len_exp =
			vht_cap_info->ampdu_len_exponent & 0x7;
	he_6g_cap->max_mpdu_len = vht_cap_info->ampdu_len;
	he_6g_cap->sm_pow_save = ht_cap_info->mimo_power_save;
	he_6g_cap->rd_responder = 0;
	he_6g_cap->rx_ant_pattern_consistency = 0;
	he_6g_cap->tx_ant_pattern_consistency = 0;

	lim_log_he_6g_cap(mac_ctx, he_6g_cap);

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11AX_BSS_COLOR
/**
 * populate_dot11f_he_bss_color_change() - pouldate HE BSS color change IE
 * @mac_ctx: Global MAC context
 * @session: PE session
 * @he_bss_color: pointer to HE BSS color change IE
 *
 * Populdate the HE BSS color change IE based on the session.
 */
QDF_STATUS
populate_dot11f_he_bss_color_change(struct mac_context *mac_ctx,
				    struct pe_session *session,
				    tDot11fIEbss_color_change *he_bss_color)
{
	if (!session->bss_color_changing) {
		he_bss_color->present = 0;
		return QDF_STATUS_SUCCESS;
	}

	he_bss_color->present = 1;
	he_bss_color->countdown = session->he_bss_color_change.countdown;
	he_bss_color->new_color = session->he_bss_color_change.new_color;

	lim_log_he_bss_color(mac_ctx, he_bss_color);

	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_FEATURE_11AX_BSS_COLOR */
#endif /* WLAN_FEATURE_11AX */

#if defined(WLAN_FEATURE_11AX) && defined(WLAN_SUPPORT_TWT)
QDF_STATUS populate_dot11f_twt_extended_caps(struct mac_context *mac_ctx,
					     struct pe_session *pe_session,
					     tDot11fIEExtCap *dot11f)
{
	struct s_ext_cap *p_ext_cap;

	if (pe_session->opmode == QDF_STA_MODE &&
	    !pe_session->enable_session_twt_support) {
		return QDF_STATUS_SUCCESS;
	}

	dot11f->num_bytes = DOT11F_IE_EXTCAP_MAX_LEN;
	p_ext_cap = (struct s_ext_cap *)dot11f->bytes;

	if (pe_session->opmode == QDF_STA_MODE)
		p_ext_cap->twt_requestor_support =
			mac_ctx->mlme_cfg->he_caps.dot11_he_cap.twt_request &&
			mac_ctx->mlme_cfg->twt_cfg.req_flag;

	if (pe_session->opmode == QDF_SAP_MODE)
		p_ext_cap->twt_responder_support =
			mac_ctx->mlme_cfg->he_caps.dot11_he_cap.twt_responder &&
			mac_ctx->mlme_cfg->twt_cfg.res_flag;

	dot11f->num_bytes = lim_compute_ext_cap_ie_length(dot11f);

	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS populate_dot11f_btm_extended_caps(struct mac_context *mac_ctx,
					     struct pe_session *pe_session,
					     struct sDot11fIEExtCap *dot11f)
{
	struct s_ext_cap *p_ext_cap;
	QDF_STATUS  status;

	pe_debug("enter");
	dot11f->num_bytes = DOT11F_IE_EXTCAP_MAX_LEN;
	p_ext_cap = (struct s_ext_cap *)dot11f->bytes;

	status = cm_akm_roam_allowed(mac_ctx, pe_session->vdev_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		p_ext_cap->bss_transition = 0;
		pe_debug("Disable btm for roaming not suppprted");
	}

	dot11f->num_bytes = lim_compute_ext_cap_ie_length(dot11f);

	return QDF_STATUS_SUCCESS;
}

/* parser_api.c ends here. */
