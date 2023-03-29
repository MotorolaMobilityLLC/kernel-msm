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

/*
 *
 * This file lim_prop_exts_utils.cc contains the utility functions
 * to populate, parse proprietary extensions required to
 * support ANI feature set.
 *
 * Author:        Chandra Modumudi
 * Date:          11/27/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#include "ani_global.h"
#include "wni_cfg.h"
#include "sir_common.h"
#include "sir_debug.h"
#include "utils_api.h"
#include "lim_api.h"
#include "lim_types.h"
#include "lim_utils.h"
#include "lim_assoc_utils.h"
#include "lim_prop_exts_utils.h"
#include "lim_ser_des_utils.h"
#include "lim_trace.h"
#include "lim_ft_defs.h"
#include "lim_session.h"
#include "wma.h"
#include "wlan_utility.h"

#ifdef FEATURE_WLAN_ESE
/**
 * get_local_power_constraint_probe_response() - extracts local constraint
 * from probe response
 * @beacon_struct: beacon structure
 * @local_constraint: local constraint pointer
 * @session: A pointer to session entry.
 *
 * Return: None
 */
static void get_local_power_constraint_probe_response(
		tpSirProbeRespBeacon beacon_struct,
		int8_t *local_constraint,
		struct pe_session *session)
{
	if (beacon_struct->eseTxPwr.present)
		*local_constraint =
			beacon_struct->eseTxPwr.power_limit;
}

/**
 * get_ese_version_ie_probe_response() - extracts ESE version IE
 * from probe response
 * @beacon_struct: beacon structure
 * @session: A pointer to session entry.
 *
 * Return: None
 */
static void get_ese_version_ie_probe_response(struct mac_context *mac_ctx,
					tpSirProbeRespBeacon beacon_struct,
					struct pe_session *session)
{
	if (mac_ctx->mlme_cfg->lfr.ese_enabled)
		session->is_ese_version_ie_present =
			beacon_struct->is_ese_ver_ie_present;
}
#else
static void get_local_power_constraint_probe_response(
		tpSirProbeRespBeacon beacon_struct,
		int8_t *local_constraint,
		struct pe_session *session)
{

}

static inline void get_ese_version_ie_probe_response(struct mac_context *mac_ctx,
					tpSirProbeRespBeacon beacon_struct,
					struct pe_session *session)
{
}
#endif

#ifdef WLAN_FEATURE_11AX
static void lim_extract_he_op(struct pe_session *session,
		tSirProbeRespBeacon *beacon_struct)
{
	uint8_t fw_vht_ch_wd;
	uint8_t ap_bcon_ch_width;
	uint8_t center_freq_diff;

	if (!session->he_capable)
		return;
	if (!beacon_struct->he_op.present) {
		pe_debug("HE op not present in beacon");
		return;
	}
	qdf_mem_copy(&session->he_op, &beacon_struct->he_op,
			sizeof(session->he_op));
	pe_debug("he_op.bss_color %d", session->he_op.bss_color);
	pe_debug("he_op.default_pe %d", session->he_op.default_pe);
	if (!session->he_6ghz_band)
		return;
	if (!session->he_op.oper_info_6g_present) {
		pe_debug("6GHz op not present in 6G beacon");
		return;
	}
	session->ch_width = session->he_op.oper_info_6g.info.ch_width;
	session->ch_center_freq_seg0 =
		session->he_op.oper_info_6g.info.center_freq_seg0;
	session->ch_center_freq_seg1 =
		session->he_op.oper_info_6g.info.center_freq_seg1;
	session->ap_power_type =
		session->he_op.oper_info_6g.info.reg_info;
	pe_debug("6G op info: ch_wd %d cntr_freq_seg0 %d cntr_freq_seg1 %d",
		 session->ch_width, session->ch_center_freq_seg0,
		 session->ch_center_freq_seg1);

	if (!session->ch_center_freq_seg1)
		return;

	fw_vht_ch_wd = wma_get_vht_ch_width();
	if (fw_vht_ch_wd <= WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ) {
		session->ch_width = CH_WIDTH_80MHZ;
		session->ch_center_freq_seg1 = 0;
		return;
	}
	center_freq_diff = abs(session->ch_center_freq_seg1 -
			       session->ch_center_freq_seg0);
	if (center_freq_diff == 8) {
		ap_bcon_ch_width = CH_WIDTH_160MHZ;
	} else if (center_freq_diff > 16) {
		ap_bcon_ch_width = CH_WIDTH_80P80MHZ;
	} else {
		session->ch_width = CH_WIDTH_80MHZ;
		session->ch_center_freq_seg1 = 0;
		return;
	}

	if ((ap_bcon_ch_width == CH_WIDTH_80P80MHZ) &&
	    (fw_vht_ch_wd != WNI_CFG_VHT_CHANNEL_WIDTH_80_PLUS_80MHZ)) {
		session->ch_width = CH_WIDTH_80MHZ;
		session->ch_center_freq_seg1 = 0;
	}
}

static bool lim_validate_he160_mcs_map(struct mac_context *mac_ctx,
				       uint16_t peer_rx, uint16_t peer_tx,
				       uint8_t nss)
{
	uint16_t rx_he_mcs_map;
	uint16_t tx_he_mcs_map;
	uint16_t he_mcs_map;

	he_mcs_map = *((uint16_t *)mac_ctx->mlme_cfg->he_caps.dot11_he_cap.
				tx_he_mcs_map_160);
	rx_he_mcs_map = HE_INTERSECT_MCS(peer_rx, he_mcs_map);

	he_mcs_map = *((uint16_t *)mac_ctx->mlme_cfg->he_caps.dot11_he_cap.
				rx_he_mcs_map_160);
	tx_he_mcs_map = HE_INTERSECT_MCS(peer_tx, he_mcs_map);

	if (nss == NSS_1x1_MODE) {
		rx_he_mcs_map |= HE_MCS_INV_MSK_4_NSS(1);
		tx_he_mcs_map |= HE_MCS_INV_MSK_4_NSS(1);
	} else if (nss == NSS_2x2_MODE) {
		rx_he_mcs_map |= (HE_MCS_INV_MSK_4_NSS(1) &
				HE_MCS_INV_MSK_4_NSS(2));
		tx_he_mcs_map |= (HE_MCS_INV_MSK_4_NSS(1) &
				HE_MCS_INV_MSK_4_NSS(2));
	}

	return ((rx_he_mcs_map != HE_MCS_ALL_DISABLED) &&
		(tx_he_mcs_map != HE_MCS_ALL_DISABLED));
}

static void lim_check_is_he_mcs_valid(struct pe_session *session,
				      tSirProbeRespBeacon *beacon_struct)
{
	uint8_t i;
	uint16_t mcs_map;

	if (!session->he_capable || !beacon_struct->he_cap.present)
		return;

	mcs_map = beacon_struct->he_cap.rx_he_mcs_map_lt_80;
	for (i = 0; i < session->nss; i++) {
		if (((mcs_map >> (i * 2)) & 0x3) != 0x3)
			return;
	}
	session->he_capable = false;
	pe_err("AP does not have valid MCS map");
	if (session->vhtCapability) {
		session->dot11mode = MLME_DOT11_MODE_11AC;
		pe_debug("Update dot11mode to 11ac");
	} else {
		session->dot11mode = MLME_DOT11_MODE_11N;
		pe_debug("Update dot11mode to 11N");
	}
}

void lim_update_he_bw_cap_mcs(struct pe_session *session,
			      tSirProbeRespBeacon *beacon)
{
	uint8_t is_80mhz;

	if (!session->he_capable)
		return;

	if ((session->opmode == QDF_STA_MODE ||
	     session->opmode == QDF_P2P_CLIENT_MODE) &&
	    beacon && beacon->he_cap.present) {
		if (!beacon->he_cap.chan_width_2) {
			is_80mhz = 1;
		} else if (beacon->he_cap.chan_width_2 &&
			 !lim_validate_he160_mcs_map(session->mac_ctx,
			   *((uint16_t *)beacon->he_cap.rx_he_mcs_map_160),
			   *((uint16_t *)beacon->he_cap.tx_he_mcs_map_160),
						     session->nss)) {
			is_80mhz = 1;
			if (session->ch_width == CH_WIDTH_160MHZ) {
				pe_debug("HE160 Rx/Tx MCS is not valid, falling back to 80MHz");
				session->ch_width = CH_WIDTH_80MHZ;
			}
		} else {
			is_80mhz = 0;
		}
	} else {
		is_80mhz = 1;
	}

	if (session->ch_width <= CH_WIDTH_80MHZ && is_80mhz) {
		session->he_config.chan_width_2 = 0;
		session->he_config.chan_width_3 = 0;
	} else if (session->ch_width == CH_WIDTH_160MHZ) {
		session->he_config.chan_width_3 = 0;
	}
	/* Reset the > 20MHz caps for 20MHz connection */
	if (session->ch_width == CH_WIDTH_20MHZ) {
		session->he_config.chan_width_0 = 0;
		session->he_config.chan_width_1 = 0;
		session->he_config.chan_width_2 = 0;
		session->he_config.chan_width_3 = 0;
		session->he_config.chan_width_4 = 0;
		session->he_config.chan_width_5 = 0;
		session->he_config.chan_width_6 = 0;
		session->he_config.he_ppdu_20_in_40Mhz_2G = 0;
		session->he_config.he_ppdu_20_in_160_80p80Mhz = 0;
		session->he_config.he_ppdu_80_in_160_80p80Mhz = 0;
	}
	if (WLAN_REG_IS_24GHZ_CH_FREQ(session->curr_op_freq)) {
		session->he_config.chan_width_1 = 0;
		session->he_config.chan_width_2 = 0;
		session->he_config.chan_width_3 = 0;
		session->he_config.chan_width_5 = 0;
		session->he_config.chan_width_6 = 0;
	} else {
		session->he_config.chan_width_0 = 0;
		session->he_config.chan_width_4 = 0;
		session->he_config.chan_width_6 = 0;
	}
	if (!session->he_config.chan_width_2) {
		session->he_config.bfee_sts_gt_80 = 0;
		session->he_config.num_sounding_gt_80 = 0;
		session->he_config.he_ppdu_20_in_160_80p80Mhz = 0;
		session->he_config.he_ppdu_80_in_160_80p80Mhz = 0;
		*(uint16_t *)session->he_config.rx_he_mcs_map_160 =
							HE_MCS_ALL_DISABLED;
		*(uint16_t *)session->he_config.tx_he_mcs_map_160 =
							HE_MCS_ALL_DISABLED;
	}
	if (!session->he_config.chan_width_3) {
		*(uint16_t *)session->he_config.rx_he_mcs_map_80_80 =
							HE_MCS_ALL_DISABLED;
		*(uint16_t *)session->he_config.tx_he_mcs_map_80_80 =
							HE_MCS_ALL_DISABLED;
	}
}

void lim_update_he_mcs_12_13_map(struct wlan_objmgr_psoc *psoc,
				 uint8_t vdev_id, uint16_t he_mcs_12_13_map)
{
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_LEGACY_MAC_ID);
	if (!vdev) {
		pe_err("vdev not found for id: %d", vdev_id);
		return;
	}
	wlan_vdev_obj_lock(vdev);
	wlan_vdev_mlme_set_he_mcs_12_13_map(vdev, he_mcs_12_13_map);
	wlan_vdev_obj_unlock(vdev);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);
}
#else
static inline void lim_extract_he_op(struct pe_session *session,
		tSirProbeRespBeacon *beacon_struct)
{}
static void lim_check_is_he_mcs_valid(struct pe_session *session,
				      tSirProbeRespBeacon *beacon_struct)
{
}

void lim_update_he_mcs_12_13_map(struct wlan_objmgr_psoc *psoc,
				 uint8_t vdev_id, uint16_t he_mcs_12_13_map)
{
}
#endif

void lim_objmgr_update_vdev_nss(struct wlan_objmgr_psoc *psoc,
				uint8_t vdev_id, uint8_t nss)
{
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_LEGACY_MAC_ID);
	if (!vdev) {
		pe_err("vdev not found for id: %d", vdev_id);
		return;
	}
	wlan_vdev_obj_lock(vdev);
	wlan_vdev_mlme_set_nss(vdev, nss);
	wlan_vdev_obj_unlock(vdev);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);
}

#ifdef WLAN_ADAPTIVE_11R
/**
 * lim_extract_adaptive_11r_cap() - check if the AP has adaptive 11r
 * IE
 * @ie: Pointer to the IE
 * @ie_len: ie Length
 *
 * Return: True if adaptive 11r IE is present
 */
static bool lim_extract_adaptive_11r_cap(uint8_t *ie, uint16_t ie_len)
{
	const uint8_t *adaptive_ie;
	uint8_t data;
	bool adaptive_11r;

	adaptive_ie = wlan_get_vendor_ie_ptr_from_oui(LIM_ADAPTIVE_11R_OUI,
						      LIM_ADAPTIVE_11R_OUI_SIZE,
						      ie, ie_len);
	if (!adaptive_ie)
		return false;

	if ((adaptive_ie[1] < (OUI_LENGTH + 1)) ||
	    (adaptive_ie[1] > MAX_ADAPTIVE_11R_IE_LEN))
		return false;

	data = *(adaptive_ie + OUI_LENGTH + 2);
	adaptive_11r = (data & 0x1) ? true : false;

	return adaptive_11r;
}

#else
static inline bool lim_extract_adaptive_11r_cap(uint8_t *ie, uint16_t ie_len)
{
	return false;
}
#endif

#ifdef WLAN_FEATURE_11AX
static void lim_check_peer_ldpc_and_update(struct pe_session *session,
				    tSirProbeRespBeacon *beacon_struct)
{
	/*
	 * In 2.4G if AP supports HE till MCS 0-9 we can associate
	 * with HE mode instead downgrading to 11ac
	 */
	if (session->he_capable &&
	    WLAN_REG_IS_24GHZ_CH_FREQ(session->curr_op_freq) &&
	    beacon_struct->he_cap.present &&
	    lim_check_he_80_mcs11_supp(session, &beacon_struct->he_cap) &&
	    !beacon_struct->he_cap.ldpc_coding) {
		session->he_capable = false;
		pe_err("LDPC check failed for HE operation");
		if (session->vhtCapability) {
			session->dot11mode = MLME_DOT11_MODE_11AC;
			pe_debug("Update dot11mode to 11ac");
		} else {
			session->dot11mode = MLME_DOT11_MODE_11N;
			pe_debug("Update dot11mode to 11N");
		}
	}
}
#else
static void lim_check_peer_ldpc_and_update(struct pe_session *session,
					   tSirProbeRespBeacon *beacon_struct)
{}
#endif

static
void lim_update_ch_width_for_p2p_client(struct mac_context *mac,
					struct pe_session *session,
					uint32_t ch_freq)
{
	struct ch_params ch_params = {0};

	/*
	 * Some IOT AP's/P2P-GO's (e.g. make: Wireless-AC 9560160MHz as P2P GO),
	 * send beacon with 20mhz and assoc resp with 80mhz and
	 * after assoc resp, next beacon also has 80mhz.
	 * Connection is expected to happen in better possible
	 * bandwidth(80MHz in this case).
	 * Start the vdev with max supported ch_width in order to support this.
	 * It'll be downgraded to appropriate ch_width or the same would be
	 * continued based on assoc resp.
	 * Restricting this check for p2p client and 5G only and this may be
	 * extended to STA based on wider testing results with multiple AP's.
	 * Limit it to 80MHz as 80+80 is channel specific and 160MHz is not
	 * supported in p2p.
	 */
	ch_params.ch_width = CH_WIDTH_80MHZ;

	wlan_reg_set_channel_params_for_freq(mac->pdev, ch_freq, 0, &ch_params);
	if (ch_params.ch_width == CH_WIDTH_20MHZ)
		ch_params.sec_ch_offset = PHY_SINGLE_CHANNEL_CENTERED;

	session->htSupportedChannelWidthSet = ch_params.sec_ch_offset ? 1 : 0;
	session->htRecommendedTxWidthSet = session->htSupportedChannelWidthSet;
	session->htSecondaryChannelOffset = ch_params.sec_ch_offset;
	session->ch_width = ch_params.ch_width;
	session->ch_center_freq_seg0 = ch_params.center_freq_seg0;
	session->ch_center_freq_seg1 = ch_params.center_freq_seg1;
	pe_debug("Start P2P_CLI in ch freq %d max supported ch_width: %u cbmode: %u seg0: %u, seg1: %u",
		 ch_freq, ch_params.ch_width, ch_params.sec_ch_offset,
		 session->ch_center_freq_seg0, session->ch_center_freq_seg1);
}

void lim_extract_ap_capability(struct mac_context *mac_ctx, uint8_t *p_ie,
			       uint16_t ie_len, uint8_t *qos_cap,
			       uint8_t *uapsd, int8_t *local_constraint,
			       struct pe_session *session,
			       bool *is_pwr_constraint)
{
	tSirProbeRespBeacon *beacon_struct;
	uint8_t ap_bcon_ch_width;
	bool new_ch_width_dfn = false;
	tDot11fIEVHTOperation *vht_op;
	uint8_t fw_vht_ch_wd;
	uint8_t vht_ch_wd;
	uint8_t center_freq_diff;
	struct s_ext_cap *ext_cap;
	uint8_t chan_center_freq_seg1;
	tDot11fIEVHTCaps *vht_caps;
	uint8_t channel = 0;
	struct mlme_vht_capabilities_info *mlme_vht_cap;

	beacon_struct = qdf_mem_malloc(sizeof(tSirProbeRespBeacon));
	if (!beacon_struct)
		return;

	*qos_cap = 0;
	*uapsd = 0;
	pe_debug("The IE's being received:");
	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			   p_ie, ie_len);
	if (sir_parse_beacon_ie(mac_ctx, beacon_struct, p_ie,
		(uint32_t) ie_len) != QDF_STATUS_SUCCESS) {
		pe_err("sir_parse_beacon_ie failed to parse beacon");
		qdf_mem_free(beacon_struct);
		return;
	}

	mlme_vht_cap = &mac_ctx->mlme_cfg->vht_caps.vht_cap_info;
	if (beacon_struct->wmeInfoPresent ||
	    beacon_struct->wmeEdcaPresent ||
	    beacon_struct->HTCaps.present)
		LIM_BSS_CAPS_SET(WME, *qos_cap);
	if (LIM_BSS_CAPS_GET(WME, *qos_cap)
			&& beacon_struct->wsmCapablePresent)
		LIM_BSS_CAPS_SET(WSM, *qos_cap);
	if (beacon_struct->HTCaps.present)
		mac_ctx->lim.htCapabilityPresentInBeacon = 1;
	else
		mac_ctx->lim.htCapabilityPresentInBeacon = 0;

	vht_op = &beacon_struct->VHTOperation;
	vht_caps = &beacon_struct->VHTCaps;
	if (IS_BSS_VHT_CAPABLE(beacon_struct->VHTCaps) &&
			vht_op->present &&
			session->vhtCapability) {
		session->vhtCapabilityPresentInBeacon = 1;
		if (((beacon_struct->Vendor1IEPresent &&
		      beacon_struct->vendor_vht_ie.present &&
		      beacon_struct->Vendor3IEPresent)) &&
		      (((beacon_struct->VHTCaps.txMCSMap & VHT_MCS_3x3_MASK) ==
			VHT_MCS_3x3_MASK) &&
		      ((beacon_struct->VHTCaps.txMCSMap & VHT_MCS_2x2_MASK) !=
		       VHT_MCS_2x2_MASK)))
			session->vht_config.su_beam_formee = 0;
	} else {
		session->vhtCapabilityPresentInBeacon = 0;
	}

	if (session->vhtCapabilityPresentInBeacon == 1 &&
			!session->htSupportedChannelWidthSet) {
		if (!mac_ctx->mlme_cfg->vht_caps.vht_cap_info.enable_txbf_20mhz)
			session->vht_config.su_beam_formee = 0;

		if (session->opmode == QDF_P2P_CLIENT_MODE &&
		    !wlan_reg_is_24ghz_ch_freq(beacon_struct->chan_freq))
			lim_update_ch_width_for_p2p_client(
					mac_ctx, session,
					beacon_struct->chan_freq);

	} else if (session->vhtCapabilityPresentInBeacon &&
			vht_op->chanWidth) {
		/* If VHT is supported min 80 MHz support is must */
		ap_bcon_ch_width = vht_op->chanWidth;
		if (vht_caps->vht_extended_nss_bw_cap) {
			if (!vht_caps->extended_nss_bw_supp)
				chan_center_freq_seg1 =
					vht_op->chan_center_freq_seg1;
			else
				chan_center_freq_seg1 =
				beacon_struct->HTInfo.chan_center_freq_seg2;
		} else {
			chan_center_freq_seg1 = vht_op->chan_center_freq_seg1;
		}
		if (chan_center_freq_seg1 &&
		    (ap_bcon_ch_width == WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ)) {
			new_ch_width_dfn = true;
			if (chan_center_freq_seg1 >
					vht_op->chan_center_freq_seg0)
			    center_freq_diff = chan_center_freq_seg1 -
						vht_op->chan_center_freq_seg0;
			else
			    center_freq_diff = vht_op->chan_center_freq_seg0 -
						chan_center_freq_seg1;
			if (center_freq_diff == 8)
				ap_bcon_ch_width =
					WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ;
			else if (center_freq_diff > 16)
				ap_bcon_ch_width =
					WNI_CFG_VHT_CHANNEL_WIDTH_80_PLUS_80MHZ;
			else
				ap_bcon_ch_width =
					WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ;
		}

		fw_vht_ch_wd = wma_get_vht_ch_width();
		vht_ch_wd = QDF_MIN(fw_vht_ch_wd, ap_bcon_ch_width);

		if ((vht_ch_wd > WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ) &&
		    (ap_bcon_ch_width ==
		     WNI_CFG_VHT_CHANNEL_WIDTH_80_PLUS_80MHZ) &&
		    mlme_vht_cap->restricted_80p80_bw_supp) {
			if ((chan_center_freq_seg1 == 138 &&
			     vht_op->chan_center_freq_seg0 == 155) ||
			    (vht_op->chan_center_freq_seg0 == 138 &&
			     chan_center_freq_seg1 == 155))
				vht_ch_wd =
					WNI_CFG_VHT_CHANNEL_WIDTH_80_PLUS_80MHZ;
			else
				vht_ch_wd =
					WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ;
		}
		/*
		 * If the supported channel width is greater than 80MHz and
		 * AP supports Nss > 1 in 160MHz mode then connect the STA
		 * in 2x2 80MHz mode instead of connecting in 160MHz mode.
		 */
		if ((vht_ch_wd > WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ) &&
		    mac_ctx->mlme_cfg->sta.sta_prefer_80mhz_over_160mhz) {
			if (!(IS_VHT_NSS_1x1(beacon_struct->VHTCaps.txMCSMap))
					&&
			   (!IS_VHT_NSS_1x1(beacon_struct->VHTCaps.rxMCSMap)))
				vht_ch_wd = WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ;
		}
		/*
		 * VHT OP IE old definition:
		 * vht_op->chan_center_freq_seg0: center freq of 80MHz/160MHz/
		 * primary 80 in 80+80MHz.
		 *
		 * vht_op->chan_center_freq_seg1: center freq of secondary 80
		 * in 80+80MHz.
		 *
		 * VHT OP IE NEW definition:
		 * vht_op->chan_center_freq_seg0: center freq of 80MHz/primary
		 * 80 in 80+80MHz/center freq of the 80 MHz channel segment
		 * that contains the primary channel in 160MHz mode.
		 *
		 * vht_op->chan_center_freq_seg1: center freq of secondary 80
		 * in 80+80MHz/center freq of 160MHz.
		 */
		session->ch_center_freq_seg0 = vht_op->chan_center_freq_seg0;
		session->ch_center_freq_seg1 = chan_center_freq_seg1;
		channel = wlan_reg_freq_to_chan(mac_ctx->pdev,
						beacon_struct->chan_freq);
		if (vht_ch_wd == WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ) {
			/* DUT or AP supports only 160MHz */
			if (ap_bcon_ch_width ==
					WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ) {
				/* AP is in 160MHz mode */
				if (!new_ch_width_dfn) {
					session->ch_center_freq_seg1 =
						vht_op->chan_center_freq_seg0;
					session->ch_center_freq_seg0 =
						lim_get_80Mhz_center_channel(channel);
				}
			} else {
				/* DUT supports only 160MHz and AP is
				 * in 80+80 mode
				 */
				vht_ch_wd = WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ;
				session->ch_center_freq_seg1 = 0;
				session->ch_center_freq_seg0 =
					lim_get_80Mhz_center_channel(channel);
			}
		} else if (vht_ch_wd == WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ) {
			/* DUT or AP supports only 80MHz */
			session->ch_center_freq_seg0 =
				lim_get_80Mhz_center_channel(channel);
			session->ch_center_freq_seg1 = 0;
		}
		session->ch_width = vht_ch_wd + 1;
	}

	if (session->vhtCapability &&
		session->vhtCapabilityPresentInBeacon &&
		beacon_struct->ext_cap.present) {
		ext_cap = (struct s_ext_cap *)beacon_struct->ext_cap.bytes;
		session->gLimOperatingMode.present =
			ext_cap->oper_mode_notification;
		if (ext_cap->oper_mode_notification) {
			uint8_t self_nss = 0;

			if (!wlan_reg_is_24ghz_ch_freq(session->curr_op_freq))
				self_nss = mac_ctx->vdev_type_nss_5g.sta;
			else
				self_nss = mac_ctx->vdev_type_nss_2g.sta;

			if (CH_WIDTH_160MHZ > session->ch_width)
				session->gLimOperatingMode.chanWidth =
						session->ch_width;
			else
				session->gLimOperatingMode.chanWidth =
					CH_WIDTH_160MHZ;
			/** Populate vdev nss in OMN ie of assoc requse for
			 *  WFA CERT test scenario.
			 */
			if (ext_cap->beacon_protection_enable &&
			    (session->opmode == QDF_STA_MODE) &&
			    (!session->nss_forced_1x1) &&
			     lim_get_nss_supported_by_ap(
					&beacon_struct->VHTCaps,
					&beacon_struct->HTCaps,
					&beacon_struct->he_cap) ==
						 NSS_1x1_MODE)
				session->gLimOperatingMode.rxNSS = self_nss - 1;
			else
				session->gLimOperatingMode.rxNSS =
							session->nss - 1;
		} else {
			pe_err("AP does not support op_mode rx");
		}
	}
	lim_check_is_he_mcs_valid(session, beacon_struct);
	lim_check_peer_ldpc_and_update(session, beacon_struct);
	lim_extract_he_op(session, beacon_struct);
	lim_update_he_bw_cap_mcs(session, beacon_struct);
	/* Extract the UAPSD flag from WMM Parameter element */
	if (beacon_struct->wmeEdcaPresent)
		*uapsd = beacon_struct->edcaParams.qosInfo.uapsd;

	if (mac_ctx->mlme_cfg->sta.allow_tpc_from_ap) {
		if (beacon_struct->powerConstraintPresent) {
			*local_constraint =
				beacon_struct->localPowerConstraint.
					localPowerConstraints;
			*is_pwr_constraint = true;
		} else {
			get_local_power_constraint_probe_response(
				beacon_struct, local_constraint, session);
			*is_pwr_constraint = false;
		}
	}

	get_ese_version_ie_probe_response(mac_ctx, beacon_struct, session);

	session->country_info_present = false;
	/* Initializing before first use */
	if (beacon_struct->countryInfoPresent)
		session->country_info_present = true;
	/* Check if Extended caps are present in probe resp or not */
	if (beacon_struct->ext_cap.present)
		session->is_ext_caps_present = true;
	/* Update HS 2.0 Information Element */
	if (beacon_struct->hs20vendor_ie.present) {
		pe_debug("HS20 Indication Element Present, rel#: %u id: %u",
			beacon_struct->hs20vendor_ie.release_num,
			beacon_struct->hs20vendor_ie.hs_id_present);
		qdf_mem_copy(&session->hs20vendor_ie,
			&beacon_struct->hs20vendor_ie,
			sizeof(tDot11fIEhs20vendor_ie) -
			sizeof(beacon_struct->hs20vendor_ie.hs_id));
		if (beacon_struct->hs20vendor_ie.hs_id_present)
			qdf_mem_copy(&session->hs20vendor_ie.hs_id,
				&beacon_struct->hs20vendor_ie.hs_id,
				sizeof(beacon_struct->hs20vendor_ie.hs_id));
	}

	lim_objmgr_update_vdev_nss(mac_ctx->psoc, session->smeSessionId,
				   session->nss);

	session->is_adaptive_11r_connection =
			lim_extract_adaptive_11r_cap(p_ie, ie_len);
	qdf_mem_free(beacon_struct);
	return;
} /****** end lim_extract_ap_capability() ******/

/**
 * lim_get_htcb_state
 *
 ***FUNCTION:
 * This routing provides the translation of Airgo Enum to HT enum for determining
 * secondary channel offset.
 * Airgo Enum is required for backward compatibility purposes.
 *
 *
 ***NOTE:
 *
 * @param  mac - Pointer to Global MAC structure
 * @return The corresponding HT enumeration
 */
ePhyChanBondState lim_get_htcb_state(ePhyChanBondState aniCBMode)
{
	switch (aniCBMode) {
	case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW:
	case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED:
	case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH:
	case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
		return PHY_DOUBLE_CHANNEL_HIGH_PRIMARY;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW:
	case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED:
	case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH:
	case PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
		return PHY_DOUBLE_CHANNEL_LOW_PRIMARY;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED:
		return PHY_SINGLE_CHANNEL_CENTERED;
	default:
		return PHY_SINGLE_CHANNEL_CENTERED;
	}
}
