/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
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
/**
 * DOC: define public APIs exposed by the mlme component
 */

#include "cfg_ucfg_api.h"
#include "wlan_mlme_main.h"
#include "wlan_mlme_ucfg_api.h"
#include "wma_types.h"
#include "wmi_unified.h"
#include "wma.h"
#include "wma_internal.h"
#include "wlan_crypto_global_api.h"
#include "wlan_utility.h"
#include "wlan_policy_mgr_ucfg.h"

QDF_STATUS wlan_mlme_get_cfg_str(uint8_t *dst, struct mlme_cfg_str *cfg_str,
				 qdf_size_t *len)
{
	if (*len < cfg_str->len) {
		mlme_legacy_err("Invalid len %zd", *len);
		return QDF_STATUS_E_INVAL;
	}

	*len = cfg_str->len;
	qdf_mem_copy(dst, cfg_str->data, *len);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_cfg_str(uint8_t *src, struct mlme_cfg_str *dst_cfg_str,
				 qdf_size_t len)
{
	if (len > dst_cfg_str->max_len) {
		mlme_legacy_err("Invalid len %zd (>%zd)", len,
				dst_cfg_str->max_len);
		return QDF_STATUS_E_INVAL;
	}

	dst_cfg_str->len = len;
	qdf_mem_copy(dst_cfg_str->data, src, len);

	return QDF_STATUS_SUCCESS;
}

uint8_t wlan_mlme_get_tx_power(struct wlan_objmgr_psoc *psoc,
			       enum band_info band)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return 0;

	switch (band) {
	case BAND_2G:
		return mlme_obj->cfg.power.tx_power_2g;
	case BAND_5G:
		return mlme_obj->cfg.power.tx_power_5g;
	default:
		break;
	}
	return 0;
}

char *wlan_mlme_get_power_usage(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return NULL;

	return mlme_obj->cfg.power.power_usage.data;
}

QDF_STATUS wlan_mlme_get_ht_cap_info(struct wlan_objmgr_psoc *psoc,
				     struct mlme_ht_capabilities_info
				     *ht_cap_info)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*ht_cap_info = mlme_obj->cfg.ht_caps.ht_cap_info;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_ht_cap_info(struct wlan_objmgr_psoc *psoc,
				     struct mlme_ht_capabilities_info
				     ht_cap_info)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.ht_caps.ht_cap_info = ht_cap_info;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_max_amsdu_num(struct wlan_objmgr_psoc *psoc,
				       uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.ht_caps.max_num_amsdu;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_max_amsdu_num(struct wlan_objmgr_psoc *psoc,
				       uint8_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	if (!cfg_in_range(CFG_MAX_AMSDU_NUM, value)) {
		mlme_legacy_err("Error in setting Max AMSDU Num");
		return QDF_STATUS_E_INVAL;
	}

	mlme_obj->cfg.ht_caps.max_num_amsdu = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_ht_mpdu_density(struct wlan_objmgr_psoc *psoc,
					 uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = (uint8_t)mlme_obj->cfg.ht_caps.ampdu_params.mpdu_density;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_ht_mpdu_density(struct wlan_objmgr_psoc *psoc,
					 uint8_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	if (!cfg_in_range(CFG_MPDU_DENSITY, value)) {
		mlme_legacy_err("Invalid value %d", value);
		return QDF_STATUS_E_INVAL;
	}

	mlme_obj->cfg.ht_caps.ampdu_params.mpdu_density = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_band_capability(struct wlan_objmgr_psoc *psoc,
					 uint32_t *band_capability)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*band_capability = mlme_obj->cfg.gen.band_capability;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_band_capability(struct wlan_objmgr_psoc *psoc,
					 uint32_t band_capability)

{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.gen.band_capability = band_capability;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_prevent_link_down(struct wlan_objmgr_psoc *psoc,
					   bool *prevent_link_down)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*prevent_link_down = mlme_obj->cfg.gen.prevent_link_down;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_select_5ghz_margin(struct wlan_objmgr_psoc *psoc,
					    uint8_t *select_5ghz_margin)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*select_5ghz_margin = mlme_obj->cfg.gen.select_5ghz_margin;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_rtt_mac_randomization(struct wlan_objmgr_psoc *psoc,
					       bool *rtt_mac_randomization)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*rtt_mac_randomization = mlme_obj->cfg.gen.rtt_mac_randomization;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_crash_inject(struct wlan_objmgr_psoc *psoc,
				      bool *crash_inject)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*crash_inject = mlme_obj->cfg.gen.crash_inject;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_lpass_support(struct wlan_objmgr_psoc *psoc,
				       bool *lpass_support)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*lpass_support = mlme_obj->cfg.gen.lpass_support;

	return QDF_STATUS_SUCCESS;
}

void wlan_mlme_get_wls_6ghz_cap(struct wlan_objmgr_psoc *psoc,
				bool *wls_6ghz_capable)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*wls_6ghz_capable = cfg_default(CFG_WLS_6GHZ_CAPABLE);
		return;
	}
	*wls_6ghz_capable = mlme_obj->cfg.gen.wls_6ghz_capable;
}

QDF_STATUS wlan_mlme_get_self_recovery(struct wlan_objmgr_psoc *psoc,
				       bool *self_recovery)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*self_recovery = mlme_obj->cfg.gen.self_recovery;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sub_20_chan_width(struct wlan_objmgr_psoc *psoc,
					   uint8_t *sub_20_chan_width)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*sub_20_chan_width = mlme_obj->cfg.gen.sub_20_chan_width;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_fw_timeout_crash(struct wlan_objmgr_psoc *psoc,
					  bool *fw_timeout_crash)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*fw_timeout_crash = mlme_obj->cfg.gen.fw_timeout_crash;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_ito_repeat_count(struct wlan_objmgr_psoc *psoc,
					  uint8_t *ito_repeat_count)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*ito_repeat_count = mlme_obj->cfg.gen.ito_repeat_count;

	return QDF_STATUS_SUCCESS;
}

void wlan_mlme_get_sap_inactivity_override(struct wlan_objmgr_psoc *psoc,
					   bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return;

	*val = mlme_obj->cfg.qos_mlme_params.sap_max_inactivity_override;
}

QDF_STATUS wlan_mlme_get_acs_with_more_param(struct wlan_objmgr_psoc *psoc,
					     bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.acs.is_acs_with_more_param;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_auto_channel_weight(struct wlan_objmgr_psoc *psoc,
					     uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj) {
		*value = cfg_default(CFG_AUTO_CHANNEL_SELECT_WEIGHT);
		return QDF_STATUS_E_FAILURE;
	}

	*value = mlme_obj->cfg.acs.auto_channel_select_weight;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_vendor_acs_support(struct wlan_objmgr_psoc *psoc,
					    bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.acs.is_vendor_acs_support;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_acs_support_for_dfs_ltecoex(struct wlan_objmgr_psoc *psoc,
					  bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.acs.is_acs_support_for_dfs_ltecoex;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_external_acs_policy(struct wlan_objmgr_psoc *psoc,
				  bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.acs.is_external_acs_policy;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_tx_chainmask_cck(struct wlan_objmgr_psoc *psoc,
					  bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.chainmask_cfg.tx_chain_mask_cck;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_tx_chainmask_1ss(struct wlan_objmgr_psoc *psoc,
					  uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.chainmask_cfg.tx_chain_mask_1ss;
	return QDF_STATUS_SUCCESS;
}

void
wlan_mlme_update_cfg_with_tgt_caps(struct wlan_objmgr_psoc *psoc,
				   struct mlme_tgt_caps *tgt_caps)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return;

	/* Update the mlme cfg according to the tgt capability received */

	mlme_obj->cfg.gen.data_stall_recovery_fw_support =
				tgt_caps->data_stall_recovery_fw_support;

	mlme_obj->cfg.gen.bigtk_support = tgt_caps->bigtk_support;
	mlme_obj->cfg.gen.stop_all_host_scan_support =
			tgt_caps->stop_all_host_scan_support;
	mlme_obj->cfg.gen.dual_sta_roam_fw_support =
			tgt_caps->dual_sta_roam_fw_support;
	mlme_obj->cfg.gen.ocv_support = tgt_caps->ocv_support;
}

#ifdef WLAN_FEATURE_11AX
QDF_STATUS wlan_mlme_cfg_get_he_ul_mumimo(struct wlan_objmgr_psoc *psoc,
					  uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.he_caps.dot11_he_cap.ul_mu;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_cfg_get_he_caps(struct wlan_objmgr_psoc *psoc,
				tDot11fIEhe_cap *he_cap)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*he_cap = mlme_obj->cfg.he_caps.he_cap_orig;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_cfg_set_he_ul_mumimo(struct wlan_objmgr_psoc *psoc,
					  uint32_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	if (!cfg_in_range(CFG_HE_UL_MUMIMO, value)) {
		mlme_legacy_debug("Failed to set CFG_HE_UL_MUMIMO with %d",
				  value);
		return QDF_STATUS_E_FAILURE;
	}

	mlme_obj->cfg.he_caps.dot11_he_cap.ul_mu = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_cfg_get_enable_ul_mimo(struct wlan_objmgr_psoc *psoc,
					    uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.he_caps.enable_ul_mimo;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_cfg_get_enable_ul_ofdm(struct wlan_objmgr_psoc *psoc,
					    uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;


	*value = mlme_obj->cfg.he_caps.enable_ul_ofdm;

	return QDF_STATUS_SUCCESS;
}

/* mlme_get_min_rate_cap() - get minimum capability for HE-MCS between
 *                           ini value and fw capability.
 *
 * Rx HE-MCS Map and Tx HE-MCS Map subfields format where 2-bit indicates
 * 0 indicates support for HE-MCS 0-7 for n spatial streams
 * 1 indicates support for HE-MCS 0-9 for n spatial streams
 * 2 indicates support for HE-MCS 0-11 for n spatial streams
 * 3 indicates that n spatial streams is not supported for HE PPDUs
 *
 */
static uint16_t mlme_get_min_rate_cap(uint16_t val1, uint16_t val2)
{
	uint16_t ret = 0, i;

	for (i = 0; i < 8; i++) {
		if (((val1 >> (2 * i)) & 0x3) == 0x3 ||
		    ((val2 >> (2 * i)) & 0x3) == 0x3) {
			ret |= 0x3 << (2 * i);
			continue;
		}
		ret |= QDF_MIN((val1 >> (2 * i)) & 0x3,
			      (val2 >> (2 * i)) & 0x3) << (2 * i);
	}
	return ret;
}

QDF_STATUS mlme_update_tgt_he_caps_in_cfg(struct wlan_objmgr_psoc *psoc,
					  struct wma_tgt_cfg *wma_cfg)
{
	uint8_t chan_width;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tDot11fIEhe_cap *he_cap = &wma_cfg->he_cap;
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);
	uint8_t value;
	uint16_t tx_mcs_map = 0;
	uint16_t rx_mcs_map = 0;
	uint8_t nss;

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.he_caps.dot11_he_cap.present = 1;
	mlme_obj->cfg.he_caps.dot11_he_cap.htc_he = he_cap->htc_he;

	value = QDF_MIN(he_cap->twt_request,
			mlme_obj->cfg.he_caps.dot11_he_cap.twt_request);
	mlme_obj->cfg.he_caps.dot11_he_cap.twt_request = value;

	value = QDF_MIN(he_cap->fragmentation,
			mlme_obj->cfg.he_caps.he_dynamic_fragmentation);

	if (cfg_in_range(CFG_HE_FRAGMENTATION, value))
		mlme_obj->cfg.he_caps.dot11_he_cap.fragmentation = value;

	if (cfg_in_range(CFG_HE_MAX_FRAG_MSDU,
			 he_cap->max_num_frag_msdu_amsdu_exp))
		mlme_obj->cfg.he_caps.dot11_he_cap.max_num_frag_msdu_amsdu_exp =
					he_cap->max_num_frag_msdu_amsdu_exp;
	if (cfg_in_range(CFG_HE_MIN_FRAG_SIZE, he_cap->min_frag_size))
		mlme_obj->cfg.he_caps.dot11_he_cap.min_frag_size =
					he_cap->min_frag_size;
	if (cfg_in_range(CFG_HE_TRIG_PAD, he_cap->trigger_frm_mac_pad))
		mlme_obj->cfg.he_caps.dot11_he_cap.trigger_frm_mac_pad =
			QDF_MIN(he_cap->trigger_frm_mac_pad,
				mlme_obj->cfg.he_caps.dot11_he_cap.trigger_frm_mac_pad);
	if (cfg_in_range(CFG_HE_MTID_AGGR_RX, he_cap->multi_tid_aggr_rx_supp))
		mlme_obj->cfg.he_caps.dot11_he_cap.multi_tid_aggr_rx_supp =
					he_cap->multi_tid_aggr_rx_supp;
	if (cfg_in_range(CFG_HE_MTID_AGGR_TX, he_cap->multi_tid_aggr_tx_supp))
		mlme_obj->cfg.he_caps.dot11_he_cap.multi_tid_aggr_tx_supp =
					he_cap->multi_tid_aggr_tx_supp;
	if (cfg_in_range(CFG_HE_LINK_ADAPTATION, he_cap->he_link_adaptation))
		mlme_obj->cfg.he_caps.dot11_he_cap.he_link_adaptation =
					he_cap->he_link_adaptation;
	mlme_obj->cfg.he_caps.dot11_he_cap.all_ack = he_cap->all_ack;
	mlme_obj->cfg.he_caps.dot11_he_cap.trigd_rsp_sched =
					he_cap->trigd_rsp_sched;
	mlme_obj->cfg.he_caps.dot11_he_cap.a_bsr = he_cap->a_bsr;

	value = QDF_MIN(he_cap->broadcast_twt,
			mlme_obj->cfg.he_caps.dot11_he_cap.broadcast_twt);
	mlme_obj->cfg.he_caps.dot11_he_cap.broadcast_twt = value;

	mlme_obj->cfg.he_caps.dot11_he_cap.flex_twt_sched =
			he_cap->flex_twt_sched;

	mlme_obj->cfg.he_caps.dot11_he_cap.ba_32bit_bitmap =
					he_cap->ba_32bit_bitmap;
	mlme_obj->cfg.he_caps.dot11_he_cap.mu_cascade = he_cap->mu_cascade;
	mlme_obj->cfg.he_caps.dot11_he_cap.ack_enabled_multitid =
					he_cap->ack_enabled_multitid;
	mlme_obj->cfg.he_caps.dot11_he_cap.omi_a_ctrl = he_cap->omi_a_ctrl;
	mlme_obj->cfg.he_caps.dot11_he_cap.ofdma_ra = he_cap->ofdma_ra;
	if (cfg_in_range(CFG_HE_MAX_AMPDU_LEN, he_cap->max_ampdu_len_exp_ext))
		mlme_obj->cfg.he_caps.dot11_he_cap.max_ampdu_len_exp_ext =
					he_cap->max_ampdu_len_exp_ext;
	mlme_obj->cfg.he_caps.dot11_he_cap.amsdu_frag = he_cap->amsdu_frag;
	mlme_obj->cfg.he_caps.dot11_he_cap.rx_ctrl_frame =
					he_cap->rx_ctrl_frame;
	mlme_obj->cfg.he_caps.dot11_he_cap.bsrp_ampdu_aggr =
					he_cap->bsrp_ampdu_aggr;
	mlme_obj->cfg.he_caps.dot11_he_cap.qtp = he_cap->qtp;
	mlme_obj->cfg.he_caps.dot11_he_cap.a_bqr = he_cap->a_bqr;
	mlme_obj->cfg.he_caps.dot11_he_cap.spatial_reuse_param_rspder =
					he_cap->spatial_reuse_param_rspder;
	mlme_obj->cfg.he_caps.dot11_he_cap.ndp_feedback_supp =
					he_cap->ndp_feedback_supp;
	mlme_obj->cfg.he_caps.dot11_he_cap.ops_supp = he_cap->ops_supp;
	mlme_obj->cfg.he_caps.dot11_he_cap.amsdu_in_ampdu =
					he_cap->amsdu_in_ampdu;
	mlme_obj->cfg.he_caps.dot11_he_cap.he_sub_ch_sel_tx_supp =
					he_cap->he_sub_ch_sel_tx_supp;
	mlme_obj->cfg.he_caps.dot11_he_cap.ul_2x996_tone_ru_supp =
					he_cap->ul_2x996_tone_ru_supp;
	mlme_obj->cfg.he_caps.dot11_he_cap.om_ctrl_ul_mu_data_dis_rx =
					he_cap->om_ctrl_ul_mu_data_dis_rx;
	mlme_obj->cfg.he_caps.dot11_he_cap.he_dynamic_smps =
					he_cap->he_dynamic_smps;
	mlme_obj->cfg.he_caps.dot11_he_cap.punctured_sounding_supp =
					he_cap->punctured_sounding_supp;
	mlme_obj->cfg.he_caps.dot11_he_cap.ht_vht_trg_frm_rx_supp =
					he_cap->ht_vht_trg_frm_rx_supp;

	chan_width = HE_CH_WIDTH_COMBINE(he_cap->chan_width_0,
					 he_cap->chan_width_1,
					 he_cap->chan_width_2,
					 he_cap->chan_width_3,
					 he_cap->chan_width_4,
					 he_cap->chan_width_5,
					 he_cap->chan_width_6);
	if (cfg_in_range(CFG_HE_CHAN_WIDTH, chan_width)) {
		mlme_obj->cfg.he_caps.dot11_he_cap.chan_width_0 =
						he_cap->chan_width_0;
		mlme_obj->cfg.he_caps.dot11_he_cap.chan_width_1 =
						he_cap->chan_width_1;
		mlme_obj->cfg.he_caps.dot11_he_cap.chan_width_2 =
						he_cap->chan_width_2;
		mlme_obj->cfg.he_caps.dot11_he_cap.chan_width_3 =
						he_cap->chan_width_3;
		mlme_obj->cfg.he_caps.dot11_he_cap.chan_width_4 =
						he_cap->chan_width_4;
		mlme_obj->cfg.he_caps.dot11_he_cap.chan_width_5 =
						he_cap->chan_width_5;
		mlme_obj->cfg.he_caps.dot11_he_cap.chan_width_6 =
						he_cap->chan_width_6;
	}
	if (cfg_in_range(CFG_HE_RX_PREAM_PUNC, he_cap->rx_pream_puncturing))
		mlme_obj->cfg.he_caps.dot11_he_cap.rx_pream_puncturing =
				he_cap->rx_pream_puncturing;
	mlme_obj->cfg.he_caps.dot11_he_cap.device_class = he_cap->device_class;
	mlme_obj->cfg.he_caps.dot11_he_cap.ldpc_coding = he_cap->ldpc_coding;
	if (cfg_in_range(CFG_HE_LTF_PPDU, he_cap->he_1x_ltf_800_gi_ppdu))
		mlme_obj->cfg.he_caps.dot11_he_cap.he_1x_ltf_800_gi_ppdu =
					he_cap->he_1x_ltf_800_gi_ppdu;
	if (cfg_in_range(CFG_HE_MIDAMBLE_RX_MAX_NSTS,
			 he_cap->midamble_tx_rx_max_nsts))
		mlme_obj->cfg.he_caps.dot11_he_cap.midamble_tx_rx_max_nsts =
					he_cap->midamble_tx_rx_max_nsts;
	mlme_obj->cfg.he_caps.dot11_he_cap.he_4x_ltf_3200_gi_ndp =
					he_cap->he_4x_ltf_3200_gi_ndp;
	if (mlme_obj->cfg.vht_caps.vht_cap_info.rx_stbc) {
		mlme_obj->cfg.he_caps.dot11_he_cap.rx_stbc_lt_80mhz =
					he_cap->rx_stbc_lt_80mhz;
		mlme_obj->cfg.he_caps.dot11_he_cap.rx_stbc_gt_80mhz =
					he_cap->rx_stbc_gt_80mhz;
	} else {
		mlme_obj->cfg.he_caps.dot11_he_cap.rx_stbc_lt_80mhz = 0;
		mlme_obj->cfg.he_caps.dot11_he_cap.rx_stbc_gt_80mhz = 0;
	}
	if (mlme_obj->cfg.vht_caps.vht_cap_info.tx_stbc) {
		mlme_obj->cfg.he_caps.dot11_he_cap.tb_ppdu_tx_stbc_lt_80mhz =
					he_cap->tb_ppdu_tx_stbc_lt_80mhz;
		mlme_obj->cfg.he_caps.dot11_he_cap.tb_ppdu_tx_stbc_gt_80mhz =
					he_cap->tb_ppdu_tx_stbc_gt_80mhz;
	} else {
		mlme_obj->cfg.he_caps.dot11_he_cap.tb_ppdu_tx_stbc_lt_80mhz = 0;
		mlme_obj->cfg.he_caps.dot11_he_cap.tb_ppdu_tx_stbc_gt_80mhz = 0;
	}

	if (cfg_in_range(CFG_HE_DOPPLER, he_cap->doppler))
		mlme_obj->cfg.he_caps.dot11_he_cap.doppler = he_cap->doppler;
	if (cfg_in_range(CFG_HE_DCM_TX, he_cap->dcm_enc_tx))
		mlme_obj->cfg.he_caps.dot11_he_cap.dcm_enc_tx =
						he_cap->dcm_enc_tx;
	if (cfg_in_range(CFG_HE_DCM_RX, he_cap->dcm_enc_rx))
		mlme_obj->cfg.he_caps.dot11_he_cap.dcm_enc_rx =
						he_cap->dcm_enc_rx;
	mlme_obj->cfg.he_caps.dot11_he_cap.ul_he_mu = he_cap->ul_he_mu;
	if (mlme_obj->cfg.vht_caps.vht_cap_info.su_bformer) {
		mlme_obj->cfg.he_caps.dot11_he_cap.su_beamformer =
					he_cap->su_beamformer;
		if (cfg_in_range(CFG_HE_NUM_SOUND_LT80,
				 he_cap->num_sounding_lt_80))
			mlme_obj->cfg.he_caps.dot11_he_cap.num_sounding_lt_80 =
						he_cap->num_sounding_lt_80;
		if (cfg_in_range(CFG_HE_NUM_SOUND_GT80,
				 he_cap->num_sounding_gt_80))
			mlme_obj->cfg.he_caps.dot11_he_cap.num_sounding_gt_80 =
						he_cap->num_sounding_gt_80;
		mlme_obj->cfg.he_caps.dot11_he_cap.mu_beamformer =
					he_cap->mu_beamformer;

	} else {
		mlme_obj->cfg.he_caps.dot11_he_cap.su_beamformer = 0;
		mlme_obj->cfg.he_caps.dot11_he_cap.num_sounding_lt_80 = 0;
		mlme_obj->cfg.he_caps.dot11_he_cap.num_sounding_gt_80 = 0;
		mlme_obj->cfg.he_caps.dot11_he_cap.mu_beamformer = 0;
	}

	if (mlme_obj->cfg.vht_caps.vht_cap_info.su_bformee) {
		mlme_obj->cfg.he_caps.dot11_he_cap.su_beamformee =
					he_cap->su_beamformee;
		if (cfg_in_range(CFG_HE_BFEE_STS_LT80, he_cap->bfee_sts_lt_80))
			mlme_obj->cfg.he_caps.dot11_he_cap.bfee_sts_lt_80 =
						he_cap->bfee_sts_lt_80;
		if (cfg_in_range(CFG_HE_BFEE_STS_GT80, he_cap->bfee_sts_gt_80))
			mlme_obj->cfg.he_caps.dot11_he_cap.bfee_sts_gt_80 =
						he_cap->bfee_sts_gt_80;

	} else {
		mlme_obj->cfg.he_caps.dot11_he_cap.su_beamformee = 0;
		mlme_obj->cfg.he_caps.dot11_he_cap.bfee_sts_lt_80 = 0;
		mlme_obj->cfg.he_caps.dot11_he_cap.bfee_sts_gt_80 = 0;
	}
	mlme_obj->cfg.he_caps.dot11_he_cap.ul_mu = he_cap->ul_mu;
	mlme_obj->cfg.he_caps.dot11_he_cap.su_feedback_tone16 =
					he_cap->su_feedback_tone16;
	mlme_obj->cfg.he_caps.dot11_he_cap.mu_feedback_tone16 =
					he_cap->mu_feedback_tone16;
	mlme_obj->cfg.he_caps.dot11_he_cap.codebook_su = he_cap->codebook_su;
	mlme_obj->cfg.he_caps.dot11_he_cap.codebook_mu = he_cap->codebook_mu;
	if (cfg_in_range(CFG_HE_BFRM_FEED, he_cap->beamforming_feedback))
		mlme_obj->cfg.he_caps.dot11_he_cap.beamforming_feedback =
					he_cap->beamforming_feedback;
	mlme_obj->cfg.he_caps.dot11_he_cap.he_er_su_ppdu =
					he_cap->he_er_su_ppdu;
	mlme_obj->cfg.he_caps.dot11_he_cap.dl_mu_mimo_part_bw =
					he_cap->dl_mu_mimo_part_bw;
	mlme_obj->cfg.he_caps.dot11_he_cap.ppet_present = he_cap->ppet_present;
	mlme_obj->cfg.he_caps.dot11_he_cap.srp = he_cap->srp;
	mlme_obj->cfg.he_caps.dot11_he_cap.power_boost = he_cap->power_boost;
	mlme_obj->cfg.he_caps.dot11_he_cap.he_ltf_800_gi_4x =
					he_cap->he_ltf_800_gi_4x;
	if (cfg_in_range(CFG_HE_MAX_NC, he_cap->max_nc))
		mlme_obj->cfg.he_caps.dot11_he_cap.max_nc = he_cap->max_nc;
	mlme_obj->cfg.he_caps.dot11_he_cap.er_he_ltf_800_gi_4x =
					he_cap->er_he_ltf_800_gi_4x;
	mlme_obj->cfg.he_caps.dot11_he_cap.he_ppdu_20_in_40Mhz_2G =
					he_cap->he_ppdu_20_in_40Mhz_2G;
	mlme_obj->cfg.he_caps.dot11_he_cap.he_ppdu_20_in_160_80p80Mhz =
					he_cap->he_ppdu_20_in_160_80p80Mhz;
	mlme_obj->cfg.he_caps.dot11_he_cap.he_ppdu_80_in_160_80p80Mhz =
					he_cap->he_ppdu_80_in_160_80p80Mhz;
	mlme_obj->cfg.he_caps.dot11_he_cap.er_1x_he_ltf_gi =
					he_cap->er_1x_he_ltf_gi;
	mlme_obj->cfg.he_caps.dot11_he_cap.midamble_tx_rx_1x_he_ltf =
					he_cap->midamble_tx_rx_1x_he_ltf;
	if (cfg_in_range(CFG_HE_DCM_MAX_BW, he_cap->dcm_max_bw))
		mlme_obj->cfg.he_caps.dot11_he_cap.dcm_max_bw =
					he_cap->dcm_max_bw;
	mlme_obj->cfg.he_caps.dot11_he_cap.longer_than_16_he_sigb_ofdm_sym =
					he_cap->longer_than_16_he_sigb_ofdm_sym;
	mlme_obj->cfg.he_caps.dot11_he_cap.tx_1024_qam_lt_242_tone_ru =
					he_cap->tx_1024_qam_lt_242_tone_ru;
	mlme_obj->cfg.he_caps.dot11_he_cap.rx_1024_qam_lt_242_tone_ru =
					he_cap->rx_1024_qam_lt_242_tone_ru;
	mlme_obj->cfg.he_caps.dot11_he_cap.non_trig_cqi_feedback =
					he_cap->non_trig_cqi_feedback;
	mlme_obj->cfg.he_caps.dot11_he_cap.rx_full_bw_su_he_mu_compress_sigb =
				he_cap->rx_full_bw_su_he_mu_compress_sigb;
	mlme_obj->cfg.he_caps.dot11_he_cap.rx_full_bw_su_he_mu_non_cmpr_sigb =
				he_cap->rx_full_bw_su_he_mu_non_cmpr_sigb;

	tx_mcs_map = mlme_get_min_rate_cap(
		mlme_obj->cfg.he_caps.dot11_he_cap.tx_he_mcs_map_lt_80,
		he_cap->tx_he_mcs_map_lt_80);
	rx_mcs_map = mlme_get_min_rate_cap(
		mlme_obj->cfg.he_caps.dot11_he_cap.rx_he_mcs_map_lt_80,
		he_cap->rx_he_mcs_map_lt_80);
	if (!mlme_obj->cfg.vht_caps.vht_cap_info.enable2x2) {
		nss = 2;
		tx_mcs_map = HE_SET_MCS_4_NSS(tx_mcs_map, HE_MCS_DISABLE, nss);
		rx_mcs_map = HE_SET_MCS_4_NSS(rx_mcs_map, HE_MCS_DISABLE, nss);
	}

	if (cfg_in_range(CFG_HE_RX_MCS_MAP_LT_80, rx_mcs_map))
		mlme_obj->cfg.he_caps.dot11_he_cap.rx_he_mcs_map_lt_80 =
			rx_mcs_map;
	if (cfg_in_range(CFG_HE_TX_MCS_MAP_LT_80, tx_mcs_map))
		mlme_obj->cfg.he_caps.dot11_he_cap.tx_he_mcs_map_lt_80 =
			tx_mcs_map;
	tx_mcs_map = mlme_get_min_rate_cap(
	   *((uint16_t *)mlme_obj->cfg.he_caps.dot11_he_cap.tx_he_mcs_map_160),
	   *((uint16_t *)he_cap->tx_he_mcs_map_160));
	rx_mcs_map = mlme_get_min_rate_cap(
	   *((uint16_t *)mlme_obj->cfg.he_caps.dot11_he_cap.rx_he_mcs_map_160),
	   *((uint16_t *)he_cap->rx_he_mcs_map_160));

	if (!mlme_obj->cfg.vht_caps.vht_cap_info.enable2x2) {
		nss = 2;
		tx_mcs_map = HE_SET_MCS_4_NSS(tx_mcs_map, HE_MCS_DISABLE, nss);
		rx_mcs_map = HE_SET_MCS_4_NSS(rx_mcs_map, HE_MCS_DISABLE, nss);
	}

	if (cfg_in_range(CFG_HE_RX_MCS_MAP_160, rx_mcs_map))
		qdf_mem_copy(mlme_obj->cfg.he_caps.dot11_he_cap.
			     rx_he_mcs_map_160,
			     &rx_mcs_map, sizeof(uint16_t));

	if (cfg_in_range(CFG_HE_TX_MCS_MAP_160, tx_mcs_map))
		qdf_mem_copy(mlme_obj->cfg.he_caps.dot11_he_cap.
			     tx_he_mcs_map_160,
			     &tx_mcs_map, sizeof(uint16_t));

	if (cfg_in_range(CFG_HE_RX_MCS_MAP_80_80,
			 *((uint16_t *)he_cap->rx_he_mcs_map_80_80)))
		qdf_mem_copy(mlme_obj->cfg.he_caps.dot11_he_cap.
			     rx_he_mcs_map_80_80,
			     he_cap->rx_he_mcs_map_80_80, sizeof(uint16_t));

	if (cfg_in_range(CFG_HE_TX_MCS_MAP_80_80,
			 *((uint16_t *)he_cap->tx_he_mcs_map_80_80)))
		qdf_mem_copy(mlme_obj->cfg.he_caps.dot11_he_cap.
			     tx_he_mcs_map_80_80,
			     he_cap->tx_he_mcs_map_80_80, sizeof(uint16_t));

	qdf_mem_copy(mlme_obj->cfg.he_caps.he_ppet_2g, wma_cfg->ppet_2g,
		     HE_MAX_PPET_SIZE);

	qdf_mem_copy(mlme_obj->cfg.he_caps.he_ppet_5g, wma_cfg->ppet_5g,
		     HE_MAX_PPET_SIZE);

	mlme_obj->cfg.he_caps.he_cap_orig = mlme_obj->cfg.he_caps.dot11_he_cap;
	/* Take intersection of host and FW capabilities */
	mlme_obj->cfg.he_caps.he_mcs_12_13_supp_2g &=
						  wma_cfg->he_mcs_12_13_supp_2g;
	mlme_obj->cfg.he_caps.he_mcs_12_13_supp_5g &=
						  wma_cfg->he_mcs_12_13_supp_5g;
	mlme_debug("mcs_12_13 2G: %x 5G: %x FW_cap: 2G: %x 5G: %x",
		   mlme_obj->cfg.he_caps.he_mcs_12_13_supp_2g,
		   mlme_obj->cfg.he_caps.he_mcs_12_13_supp_5g,
		   wma_cfg->he_mcs_12_13_supp_2g,
		   wma_cfg->he_mcs_12_13_supp_5g);

	return status;
}
#endif

QDF_STATUS wlan_mlme_get_num_11b_tx_chains(struct wlan_objmgr_psoc *psoc,
					   uint16_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.chainmask_cfg.num_11b_tx_chains;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_bt_chain_separation_flag(struct wlan_objmgr_psoc *psoc,
						  bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.chainmask_cfg.enable_bt_chain_separation;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_num_11ag_tx_chains(struct wlan_objmgr_psoc *psoc,
					    uint16_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.chainmask_cfg.num_11ag_tx_chains;
	return QDF_STATUS_SUCCESS;
}


static
bool wlan_mlme_configure_chain_mask_supported(struct wlan_objmgr_psoc *psoc)
{
	struct wma_caps_per_phy non_dbs_phy_cap = {0};
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);
	QDF_STATUS status;
	bool as_enabled, enable_bt_chain_sep, enable2x2;
	uint8_t dual_mac_feature;
	bool hw_dbs_2x2_cap;

	if (!mlme_obj)
		return false;

	status = wma_get_caps_for_phyidx_hwmode(&non_dbs_phy_cap,
						HW_MODE_DBS_NONE,
						CDS_BAND_ALL);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_legacy_err("couldn't get phy caps. skip chain mask programming");
		return false;
	}

	if (non_dbs_phy_cap.tx_chain_mask_2G < 3 ||
	    non_dbs_phy_cap.rx_chain_mask_2G < 3 ||
	    non_dbs_phy_cap.tx_chain_mask_5G < 3 ||
	    non_dbs_phy_cap.rx_chain_mask_5G < 3) {
		mlme_legacy_debug("firmware not capable. skip chain mask programming");
		return false;
	}

	enable_bt_chain_sep =
			mlme_obj->cfg.chainmask_cfg.enable_bt_chain_separation;
	as_enabled = mlme_obj->cfg.gen.as_enabled;
	ucfg_policy_mgr_get_dual_mac_feature(psoc, &dual_mac_feature);

	hw_dbs_2x2_cap = policy_mgr_is_hw_dbs_2x2_capable(psoc);
	enable2x2 = mlme_obj->cfg.vht_caps.vht_cap_info.enable2x2;

	if ((enable2x2 && !enable_bt_chain_sep) || as_enabled ||
	   (!hw_dbs_2x2_cap && dual_mac_feature != DISABLE_DBS_CXN_AND_SCAN)) {
		mlme_legacy_debug("Cannot configure chainmask enable_bt_chain_sep %d as_enabled %d enable2x2 %d hw_dbs_2x2_cap %d dual_mac_feature %d",
				  enable_bt_chain_sep, as_enabled, enable2x2,
				  hw_dbs_2x2_cap, dual_mac_feature);
		return false;
	}

	return true;
}

bool wlan_mlme_is_chain_mask_supported(struct wlan_objmgr_psoc *psoc)

{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return false;

	if (!wlan_mlme_configure_chain_mask_supported(psoc))
		return false;

	/* If user has configured 1x1 from INI */
	if (mlme_obj->cfg.chainmask_cfg.txchainmask1x1 != 3 ||
	    mlme_obj->cfg.chainmask_cfg.rxchainmask1x1 != 3) {
		mlme_legacy_debug("txchainmask1x1 %d rxchainmask1x1 %d",
				  mlme_obj->cfg.chainmask_cfg.txchainmask1x1,
				  mlme_obj->cfg.chainmask_cfg.rxchainmask1x1);
		return false;
	}

	return true;

}
QDF_STATUS wlan_mlme_configure_chain_mask(struct wlan_objmgr_psoc *psoc,
					  uint8_t session_id)
{
	int ret_val;
	uint8_t ch_msk_val;
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);
	bool mrc_disabled_2g_rx, mrc_disabled_2g_tx;
	bool mrc_disabled_5g_rx, mrc_disabled_5g_tx;

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_legacy_debug("txchainmask1x1: %d rxchainmask1x1: %d",
			  mlme_obj->cfg.chainmask_cfg.txchainmask1x1,
			  mlme_obj->cfg.chainmask_cfg.rxchainmask1x1);
	mlme_legacy_debug("tx_chain_mask_2g: %d, rx_chain_mask_2g: %d",
			  mlme_obj->cfg.chainmask_cfg.tx_chain_mask_2g,
			  mlme_obj->cfg.chainmask_cfg.rx_chain_mask_2g);
	mlme_legacy_debug("tx_chain_mask_5g: %d, rx_chain_mask_5g: %d",
			  mlme_obj->cfg.chainmask_cfg.tx_chain_mask_5g,
			  mlme_obj->cfg.chainmask_cfg.rx_chain_mask_5g);

	mrc_disabled_2g_rx =
	  mlme_obj->cfg.nss_chains_ini_cfg.disable_rx_mrc[NSS_CHAINS_BAND_2GHZ];
	mrc_disabled_2g_tx =
	  mlme_obj->cfg.nss_chains_ini_cfg.disable_tx_mrc[NSS_CHAINS_BAND_2GHZ];
	mrc_disabled_5g_rx =
	  mlme_obj->cfg.nss_chains_ini_cfg.disable_rx_mrc[NSS_CHAINS_BAND_5GHZ];
	mrc_disabled_5g_tx =
	  mlme_obj->cfg.nss_chains_ini_cfg.disable_tx_mrc[NSS_CHAINS_BAND_5GHZ];

	mlme_legacy_debug("MRC values TX:- 2g %d 5g %d RX:- 2g %d 5g %d",
			  mrc_disabled_2g_tx, mrc_disabled_5g_tx,
			  mrc_disabled_2g_rx, mrc_disabled_5g_rx);

	if (!wlan_mlme_configure_chain_mask_supported(psoc))
		return QDF_STATUS_E_FAILURE;


	if (mlme_obj->cfg.chainmask_cfg.txchainmask1x1) {
		ch_msk_val = mlme_obj->cfg.chainmask_cfg.txchainmask1x1;
		ret_val = wma_cli_set_command(session_id,
					      WMI_PDEV_PARAM_TX_CHAIN_MASK,
					      ch_msk_val, PDEV_CMD);
		if (ret_val)
			return QDF_STATUS_E_FAILURE;
	}

	if (mlme_obj->cfg.chainmask_cfg.rxchainmask1x1) {
		ch_msk_val = mlme_obj->cfg.chainmask_cfg.rxchainmask1x1;
		ret_val = wma_cli_set_command(session_id,
					      WMI_PDEV_PARAM_RX_CHAIN_MASK,
					      ch_msk_val, PDEV_CMD);
		if (ret_val)
			return QDF_STATUS_E_FAILURE;
	}

	if (mlme_obj->cfg.chainmask_cfg.txchainmask1x1 ||
	    mlme_obj->cfg.chainmask_cfg.rxchainmask1x1) {
		mlme_legacy_debug("band agnostic tx/rx chain mask set. skip per band chain mask");
		return QDF_STATUS_SUCCESS;
	}

	if (mlme_obj->cfg.chainmask_cfg.tx_chain_mask_2g &&
	    mrc_disabled_2g_tx) {
		ch_msk_val = mlme_obj->cfg.chainmask_cfg.tx_chain_mask_2g;
		ret_val = wma_cli_set_command(session_id,
					      WMI_PDEV_PARAM_TX_CHAIN_MASK_2G,
					      ch_msk_val, PDEV_CMD);
		if (0 != ret_val)
			return QDF_STATUS_E_FAILURE;
	}

	if (mlme_obj->cfg.chainmask_cfg.rx_chain_mask_2g &&
	    mrc_disabled_2g_rx) {
		ch_msk_val = mlme_obj->cfg.chainmask_cfg.rx_chain_mask_2g;
		ret_val = wma_cli_set_command(session_id,
					      WMI_PDEV_PARAM_RX_CHAIN_MASK_2G,
					      ch_msk_val, PDEV_CMD);
		if (0 != ret_val)
			return QDF_STATUS_E_FAILURE;
	}

	if (mlme_obj->cfg.chainmask_cfg.tx_chain_mask_5g &&
	    mrc_disabled_5g_tx) {
		ch_msk_val = mlme_obj->cfg.chainmask_cfg.tx_chain_mask_5g;
		ret_val = wma_cli_set_command(session_id,
					      WMI_PDEV_PARAM_TX_CHAIN_MASK_5G,
					      ch_msk_val, PDEV_CMD);
		if (0 != ret_val)
			return QDF_STATUS_E_FAILURE;
	}

	if (mlme_obj->cfg.chainmask_cfg.rx_chain_mask_5g &&
	    mrc_disabled_5g_rx) {
		ch_msk_val = mlme_obj->cfg.chainmask_cfg.rx_chain_mask_5g;
		ret_val = wma_cli_set_command(session_id,
					      WMI_PDEV_PARAM_RX_CHAIN_MASK_5G,
					      ch_msk_val, PDEV_CMD);
		if (0 != ret_val)
			return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_manufacturer_name(struct wlan_objmgr_psoc *psoc,
				uint8_t *pbuf, uint32_t *plen)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*plen = qdf_str_lcopy(pbuf,
			      mlme_obj->cfg.product_details.manufacturer_name,
			      *plen);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_model_number(struct wlan_objmgr_psoc *psoc,
			   uint8_t *pbuf, uint32_t *plen)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*plen = qdf_str_lcopy(pbuf,
			      mlme_obj->cfg.product_details.model_number,
			      *plen);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_model_name(struct wlan_objmgr_psoc *psoc,
			 uint8_t *pbuf, uint32_t *plen)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*plen = qdf_str_lcopy(pbuf,
			      mlme_obj->cfg.product_details.model_name,
			      *plen);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_manufacture_product_version(struct wlan_objmgr_psoc *psoc,
					  uint8_t *pbuf, uint32_t *plen)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*plen = qdf_str_lcopy(pbuf,
		     mlme_obj->cfg.product_details.manufacture_product_version,
		     *plen);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_manufacture_product_name(struct wlan_objmgr_psoc *psoc,
				       uint8_t *pbuf, uint32_t *plen)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*plen = qdf_str_lcopy(pbuf,
			mlme_obj->cfg.product_details.manufacture_product_name,
			*plen);
	return QDF_STATUS_SUCCESS;
}


void wlan_mlme_get_tl_delayed_trgr_frm_int(struct wlan_objmgr_psoc *psoc,
					   uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_TL_DELAYED_TRGR_FRM_INTERVAL);
		return;
	}

	*value = mlme_obj->cfg.wmm_params.delayed_trigger_frm_int;
}


QDF_STATUS wlan_mlme_get_wmm_dir_ac_vo(struct wlan_objmgr_psoc *psoc,
				       uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_vo.dir_ac_vo;

	return QDF_STATUS_SUCCESS;
}


QDF_STATUS wlan_mlme_get_wmm_nom_msdu_size_ac_vo(struct wlan_objmgr_psoc *psoc,
						 uint16_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_vo.nom_msdu_size_ac_vo;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_mean_data_rate_ac_vo(struct wlan_objmgr_psoc *psoc,
					uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_vo.mean_data_rate_ac_vo;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_wmm_min_phy_rate_ac_vo(struct wlan_objmgr_psoc *psoc,
						uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_vo.min_phy_rate_ac_vo;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_sba_ac_vo(struct wlan_objmgr_psoc *psoc, uint16_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		return QDF_STATUS_E_FAILURE;
	}

	*value = mlme_obj->cfg.wmm_params.ac_vo.sba_ac_vo;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_wmm_uapsd_vo_srv_intv(struct wlan_objmgr_psoc *psoc,
						uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_vo.uapsd_vo_srv_intv;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_wmm_uapsd_vo_sus_intv(struct wlan_objmgr_psoc *psoc,
						uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_vo.uapsd_vo_sus_intv;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_dir_ac_vi(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_vi.dir_ac_vi;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_wmm_nom_msdu_size_ac_vi(struct wlan_objmgr_psoc *psoc,
						 uint16_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value =
		mlme_obj->cfg.wmm_params.ac_vi.nom_msdu_size_ac_vi;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_mean_data_rate_ac_vi(struct wlan_objmgr_psoc *psoc,
					uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_vi.mean_data_rate_ac_vi;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_wmm_min_phy_rate_ac_vi(struct wlan_objmgr_psoc *psoc,
						uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_vi.min_phy_rate_ac_vi;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_wmm_sba_ac_vi(struct wlan_objmgr_psoc *psoc,
					uint16_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_vi.sba_ac_vi;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_uapsd_vi_srv_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_vi.uapsd_vi_srv_intv;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_wmm_uapsd_vi_sus_intv(struct wlan_objmgr_psoc *psoc,
						uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_vi.uapsd_vi_sus_intv;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_dir_ac_be(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_be.dir_ac_be;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_wmm_nom_msdu_size_ac_be(struct wlan_objmgr_psoc *psoc,
						 uint16_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_be.nom_msdu_size_ac_be;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_mean_data_rate_ac_be(struct wlan_objmgr_psoc *psoc,
					uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_be.mean_data_rate_ac_be;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_wmm_min_phy_rate_ac_be(struct wlan_objmgr_psoc *psoc,
						uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_be.min_phy_rate_ac_be;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_sba_ac_be(struct wlan_objmgr_psoc *psoc, uint16_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_be.sba_ac_be;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_wmm_uapsd_be_srv_intv(struct wlan_objmgr_psoc *psoc,
						uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_be.uapsd_be_srv_intv;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_wmm_uapsd_be_sus_intv(struct wlan_objmgr_psoc *psoc,
						uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_be.uapsd_be_sus_intv;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_dir_ac_bk(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_bk.dir_ac_bk;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_wmm_nom_msdu_size_ac_bk(struct wlan_objmgr_psoc *psoc,
						 uint16_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_bk.nom_msdu_size_ac_bk;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_mean_data_rate_ac_bk(struct wlan_objmgr_psoc *psoc,
					uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_bk.mean_data_rate_ac_bk;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_wmm_min_phy_rate_ac_bk(struct wlan_objmgr_psoc *psoc,
						uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_bk.min_phy_rate_ac_bk;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_sba_ac_bk(struct wlan_objmgr_psoc *psoc, uint16_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_bk.sba_ac_bk;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_uapsd_bk_srv_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_bk.uapsd_bk_srv_intv;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_uapsd_bk_sus_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.ac_bk.uapsd_bk_sus_intv;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_mode(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.wmm_config.wmm_mode;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_80211e_is_enabled(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.wmm_config.b80211e_is_enabled;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_wmm_uapsd_mask(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.wmm_config.uapsd_mask;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_implicit_qos_is_enabled(struct wlan_objmgr_psoc *psoc,
						 bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.wmm_config.bimplicit_qos_enabled;

	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_WLAN_ESE
void wlan_mlme_get_inactivity_interval(struct wlan_objmgr_psoc *psoc,
					uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_QOS_WMM_INACTIVITY_INTERVAL);
		return;
	}

	*value = mlme_obj->cfg.wmm_params.wmm_tspec_element.inactivity_intv;
}
#endif

void wlan_mlme_get_is_ts_burst_size_enable(struct wlan_objmgr_psoc *psoc,
					   bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_QOS_WMM_BURST_SIZE_DEFN);
		return;
	}

	*value = mlme_obj->cfg.wmm_params.wmm_tspec_element.burst_size_def;
}

void wlan_mlme_get_ts_info_ack_policy(struct wlan_objmgr_psoc *psoc,
				      enum mlme_ts_info_ack_policy *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_QOS_WMM_TS_INFO_ACK_POLICY);
		return;
	}

	*value = mlme_obj->cfg.wmm_params.wmm_tspec_element.ts_ack_policy;

}

QDF_STATUS
wlan_mlme_get_ts_acm_value_for_ac(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.wmm_params.wmm_tspec_element.ts_acm_is_off;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_listen_interval(struct wlan_objmgr_psoc *psoc,
					     int *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.listen_interval;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_sap_listen_interval(struct wlan_objmgr_psoc *psoc,
					     int value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	if (cfg_in_range(CFG_LISTEN_INTERVAL, value))
		mlme_obj->cfg.sap_cfg.listen_interval = value;
	else
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_assoc_sta_limit(struct wlan_objmgr_psoc *psoc,
					 int value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	if (cfg_in_range(CFG_ASSOC_STA_LIMIT, value))
		mlme_obj->cfg.sap_cfg.assoc_sta_limit = value;
	else
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_assoc_sta_limit(struct wlan_objmgr_psoc *psoc,
					 int *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.assoc_sta_limit;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_get_peer_info(struct wlan_objmgr_psoc *psoc,
					   bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.sap_get_peer_info;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_sap_get_peer_info(struct wlan_objmgr_psoc *psoc,
					   bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.sap_cfg.sap_get_peer_info = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_sap_bcast_deauth_enabled(struct wlan_objmgr_psoc *psoc,
				       bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.is_sap_bcast_deauth_enabled;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_is_6g_sap_fd_enabled(struct wlan_objmgr_psoc *psoc,
			       bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.is_6g_sap_fd_enabled;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_allow_all_channels(struct wlan_objmgr_psoc *psoc,
						bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.sap_allow_all_chan_param_name;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_max_peers(struct wlan_objmgr_psoc *psoc,
				       int *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.sap_max_no_peers;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_sap_max_peers(struct wlan_objmgr_psoc *psoc,
				       int value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	if (cfg_in_range(CFG_SAP_MAX_NO_PEERS, value))
		mlme_obj->cfg.sap_cfg.sap_max_no_peers = value;
	else
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_max_offload_peers(struct wlan_objmgr_psoc *psoc,
					       int *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.sap_max_offload_peers;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_max_offload_reorder_buffs(struct wlan_objmgr_psoc
						       *psoc, int *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.sap_max_offload_reorder_buffs;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_chn_switch_bcn_count(struct wlan_objmgr_psoc *psoc,
						  int *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.sap_ch_switch_beacon_cnt;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_chn_switch_mode(struct wlan_objmgr_psoc *psoc,
					     bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.sap_ch_switch_mode;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_internal_restart(struct wlan_objmgr_psoc *psoc,
					      bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.sap_internal_restart;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_max_modulated_dtim(struct wlan_objmgr_psoc *psoc,
						uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.max_li_modulated_dtim_time;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_chan_pref_location(struct wlan_objmgr_psoc *psoc,
						uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.sap_pref_chan_location;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_country_priority(struct wlan_objmgr_psoc *psoc,
					      bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.country_code_priority;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_reduced_beacon_interval(struct wlan_objmgr_psoc
						     *psoc, int *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_REDUCED_BEACON_INTERVAL);
		return QDF_STATUS_E_FAILURE;
	}

	*value = mlme_obj->cfg.sap_cfg.reduced_beacon_interval;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_chan_switch_rate_enabled(struct wlan_objmgr_psoc
						      *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.chan_switch_hostapd_rate_enabled_name;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_sap_force_11n_for_11ac(struct wlan_objmgr_psoc
						*psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.sap_force_11n_for_11ac;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_go_force_11n_for_11ac(struct wlan_objmgr_psoc
					       *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.go_force_11n_for_11ac;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_is_sap_11ac_override(struct wlan_objmgr_psoc *psoc,
					  bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_SAP_11AC_OVERRIDE);
		return QDF_STATUS_E_FAILURE;
	}
	*value = mlme_obj->cfg.sap_cfg.sap_11ac_override;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_is_go_11ac_override(struct wlan_objmgr_psoc *psoc,
					 bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_GO_11AC_OVERRIDE);
		return QDF_STATUS_E_FAILURE;
	}
	*value = mlme_obj->cfg.sap_cfg.go_11ac_override;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_sap_11ac_override(struct wlan_objmgr_psoc *psoc,
					   bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;
	mlme_obj->cfg.sap_cfg.sap_11ac_override = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_go_11ac_override(struct wlan_objmgr_psoc *psoc,
					  bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;
	mlme_obj->cfg.sap_cfg.go_11ac_override = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_bigtk_support(struct wlan_objmgr_psoc *psoc,
				       bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.gen.bigtk_support;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_ocv_support(struct wlan_objmgr_psoc *psoc,
				     bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.gen.ocv_support;

	return QDF_STATUS_SUCCESS;
}

bool wlan_mlme_get_host_scan_abort_support(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return false;

	return mlme_obj->cfg.gen.stop_all_host_scan_support;
}

bool wlan_mlme_get_dual_sta_roam_support(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return false;

	return mlme_obj->cfg.gen.dual_sta_roam_fw_support;
}

QDF_STATUS wlan_mlme_get_oce_sta_enabled_info(struct wlan_objmgr_psoc *psoc,
					      bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.oce.oce_sta_enabled;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_oce_sap_enabled_info(struct wlan_objmgr_psoc *psoc,
					      bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.oce.oce_sap_enabled;
	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_mlme_send_oce_flags_fw() - Send the oce flags to FW
 * @pdev: pointer to pdev object
 * @object: vdev object
 * @arg: Arguments to the handler
 *
 * Return: void
 */
static void wlan_mlme_send_oce_flags_fw(struct wlan_objmgr_pdev *pdev,
					void *object, void *arg)
{
	struct wlan_objmgr_vdev *vdev = object;
	uint8_t *updated_fw_value = arg;
	uint8_t *dynamic_fw_value = 0;
	uint8_t vdev_id;

	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE) {
		dynamic_fw_value = mlme_get_dynamic_oce_flags(vdev);
		if (!dynamic_fw_value)
			return;
		if (*updated_fw_value == *dynamic_fw_value) {
			mlme_legacy_debug("Current FW flags matches with updated value.");
			return;
		}
		*dynamic_fw_value = *updated_fw_value;
		vdev_id = wlan_vdev_get_id(vdev);
		if (wma_cli_set_command(vdev_id,
					WMI_VDEV_PARAM_ENABLE_DISABLE_OCE_FEATURES,
					*updated_fw_value, VDEV_CMD))
			mlme_legacy_err("Failed to send OCE update to FW");
	}
}

void wlan_mlme_update_oce_flags(struct wlan_objmgr_pdev *pdev)
{
	uint16_t sap_connected_peer, go_connected_peer;
	struct wlan_objmgr_psoc *psoc = NULL;
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	uint8_t updated_fw_value = 0;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return;

	sap_connected_peer =
	wlan_util_get_peer_count_for_mode(pdev, QDF_SAP_MODE);
	go_connected_peer =
	wlan_util_get_peer_count_for_mode(pdev, QDF_P2P_GO_MODE);
	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return;

	if (sap_connected_peer || go_connected_peer) {
		updated_fw_value = mlme_obj->cfg.oce.feature_bitmap;
		updated_fw_value &=
		~(WMI_VDEV_OCE_PROBE_REQUEST_RATE_FEATURE_BITMAP);
		updated_fw_value &=
		~(WMI_VDEV_OCE_PROBE_REQUEST_DEFERRAL_FEATURE_BITMAP);
		mlme_legacy_debug("Disable STA OCE probe req rate and defferal updated_fw_value :%d",
				  updated_fw_value);
	} else {
		updated_fw_value = mlme_obj->cfg.oce.feature_bitmap;
		mlme_legacy_debug("Update the STA OCE flags to default INI updated_fw_value :%d",
				  updated_fw_value);
	}

	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_VDEV_OP,
				wlan_mlme_send_oce_flags_fw,
				&updated_fw_value, 0, WLAN_MLME_NB_ID);
}

bool wlan_mlme_is_ap_prot_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return false;

	return mlme_obj->cfg.sap_protection_cfg.is_ap_prot_enabled;
}

QDF_STATUS wlan_mlme_get_ap_protection_mode(struct wlan_objmgr_psoc *psoc,
					    uint16_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_protection_cfg.ap_protection_mode;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_is_ap_obss_prot_enabled(struct wlan_objmgr_psoc *psoc,
					     bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_protection_cfg.enable_ap_obss_protection;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_rts_threshold(struct wlan_objmgr_psoc *psoc,
				       uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.threshold.rts_threshold;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_rts_threshold(struct wlan_objmgr_psoc *psoc,
				       uint32_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	tp_wma_handle wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma_handle) {
		wma_err("wma_handle is NULL");
		return QDF_STATUS_E_INVAL;
	}

	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.threshold.rts_threshold = value;
	wma_update_rts_params(wma_handle, value);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_frag_threshold(struct wlan_objmgr_psoc *psoc,
					uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.threshold.frag_threshold;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_frag_threshold(struct wlan_objmgr_psoc *psoc,
					uint32_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	tp_wma_handle wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma_handle) {
		wma_err("wma_handle is NULL");
		return QDF_STATUS_E_INVAL;
	}

	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.threshold.frag_threshold = value;
	wma_update_frag_params(wma_handle,
			       value);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_fils_enabled_info(struct wlan_objmgr_psoc *psoc,
					   bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.oce.fils_enabled;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_fils_enabled_info(struct wlan_objmgr_psoc *psoc,
					   bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.oce.fils_enabled = value;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_enable_bcast_probe_rsp(struct wlan_objmgr_psoc *psoc,
						bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.oce.enable_bcast_probe_rsp = value;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_sta_miracast_mcc_rest_time(struct wlan_objmgr_psoc *psoc,
					 uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sta.sta_miracast_mcc_rest_time;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_sap_mcc_chnl_avoid(struct wlan_objmgr_psoc *psoc,
				 uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.sap_cfg.sap_mcc_chnl_avoid;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_mcc_bcast_prob_resp(struct wlan_objmgr_psoc *psoc,
				  uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.feature_flags.mcc_bcast_prob_rsp;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_mcc_rts_cts_prot(struct wlan_objmgr_psoc *psoc,
			       uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.feature_flags.mcc_rts_cts_prot;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_mcc_feature(struct wlan_objmgr_psoc *psoc,
			  uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.feature_flags.enable_mcc;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_edca_params(struct wlan_mlme_edca_params *edca_params,
				     uint8_t *data, enum e_edca_type edca_ac)
{
	qdf_size_t len;

	switch (edca_ac) {
	case edca_ani_acbe_local:
		len = edca_params->ani_acbe_l.len;
		wlan_mlme_get_cfg_str(data, &edca_params->ani_acbe_l, &len);
		break;

	case edca_ani_acbk_local:
		len = edca_params->ani_acbk_l.len;
		wlan_mlme_get_cfg_str(data, &edca_params->ani_acbk_l, &len);
		break;

	case edca_ani_acvi_local:
		len = edca_params->ani_acvi_l.len;
		wlan_mlme_get_cfg_str(data, &edca_params->ani_acvi_l, &len);
		break;

	case edca_ani_acvo_local:
		len = edca_params->ani_acvo_l.len;
		wlan_mlme_get_cfg_str(data, &edca_params->ani_acvo_l, &len);
		break;

	case edca_ani_acbk_bcast:
		len = edca_params->ani_acbk_b.len;
		wlan_mlme_get_cfg_str(data, &edca_params->ani_acbk_b, &len);
		break;

	case edca_ani_acbe_bcast:
		len = edca_params->ani_acbe_b.len;
		wlan_mlme_get_cfg_str(data, &edca_params->ani_acbe_b, &len);
		break;

	case edca_ani_acvi_bcast:
		len = edca_params->ani_acvi_b.len;
		wlan_mlme_get_cfg_str(data, &edca_params->ani_acvi_b, &len);
		break;

	case edca_ani_acvo_bcast:
		len = edca_params->ani_acvo_b.len;
		wlan_mlme_get_cfg_str(data, &edca_params->ani_acvo_b, &len);
		break;

	case edca_wme_acbe_local:
		len = edca_params->wme_acbe_l.len;
		wlan_mlme_get_cfg_str(data, &edca_params->wme_acbe_l, &len);
		break;

	case edca_wme_acbk_local:
		len = edca_params->wme_acbk_l.len;
		wlan_mlme_get_cfg_str(data, &edca_params->wme_acbk_l, &len);
		break;

	case edca_wme_acvi_local:
		len = edca_params->wme_acvi_l.len;
		wlan_mlme_get_cfg_str(data, &edca_params->wme_acvi_l, &len);
		break;

	case edca_wme_acvo_local:
		len = edca_params->wme_acvo_l.len;
		wlan_mlme_get_cfg_str(data, &edca_params->wme_acvo_l, &len);
		break;

	case edca_wme_acbe_bcast:
		len = edca_params->wme_acbe_b.len;
		wlan_mlme_get_cfg_str(data, &edca_params->wme_acbe_b, &len);
		break;

	case edca_wme_acbk_bcast:
		len = edca_params->wme_acbk_b.len;
		wlan_mlme_get_cfg_str(data, &edca_params->wme_acbk_b, &len);
		break;

	case edca_wme_acvi_bcast:
		len = edca_params->wme_acvi_b.len;
		wlan_mlme_get_cfg_str(data, &edca_params->wme_acvi_b, &len);
		break;

	case edca_wme_acvo_bcast:
		len = edca_params->wme_acvo_b.len;
		wlan_mlme_get_cfg_str(data, &edca_params->wme_acvo_b, &len);
		break;

	case edca_etsi_acbe_local:
		len = edca_params->etsi_acbe_l.len;
		wlan_mlme_get_cfg_str(data, &edca_params->etsi_acbe_l, &len);
		break;

	case edca_etsi_acbk_local:
		len = edca_params->etsi_acbk_l.len;
		wlan_mlme_get_cfg_str(data, &edca_params->etsi_acbk_l, &len);
		break;

	case edca_etsi_acvi_local:
		len = edca_params->etsi_acvi_l.len;
		wlan_mlme_get_cfg_str(data, &edca_params->etsi_acvi_l, &len);
		break;

	case edca_etsi_acvo_local:
		len = edca_params->etsi_acvo_l.len;
		wlan_mlme_get_cfg_str(data, &edca_params->etsi_acvo_l, &len);
		break;

	case edca_etsi_acbe_bcast:
		len = edca_params->etsi_acbe_b.len;
		wlan_mlme_get_cfg_str(data, &edca_params->etsi_acbe_b, &len);
		break;

	case edca_etsi_acbk_bcast:
		len = edca_params->etsi_acbk_b.len;
		wlan_mlme_get_cfg_str(data, &edca_params->etsi_acbk_b, &len);
		break;

	case edca_etsi_acvi_bcast:
		len = edca_params->etsi_acvi_b.len;
		wlan_mlme_get_cfg_str(data, &edca_params->etsi_acvi_b, &len);
		break;

	case edca_etsi_acvo_bcast:
		len = edca_params->etsi_acvo_b.len;
		wlan_mlme_get_cfg_str(data, &edca_params->etsi_acvo_b, &len);
		break;
	default:
		mlme_legacy_err("Invalid edca access category");
		return QDF_STATUS_E_INVAL;
	}
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_get_wep_key(struct wlan_objmgr_vdev *vdev,
			    struct wlan_mlme_wep_cfg *wep_params,
			    enum wep_key_id wep_keyid, uint8_t *default_key,
			    qdf_size_t *key_len)
{
	struct wlan_crypto_key *crypto_key = NULL;

	if (wep_keyid >= WLAN_CRYPTO_MAXKEYIDX) {
		mlme_legacy_err("Incorrect wep key index %d", wep_keyid);
		return QDF_STATUS_E_INVAL;
	}
	crypto_key = wlan_crypto_get_key(vdev, wep_keyid);
	if (!crypto_key) {
		mlme_legacy_err("Crypto KEY not present");
		return QDF_STATUS_E_INVAL;
	}

	if (crypto_key->keylen > WLAN_CRYPTO_KEY_WEP104_LEN) {
		mlme_legacy_err("Key too large to hold");
		return QDF_STATUS_E_INVAL;
	}
	*key_len = crypto_key->keylen;
	qdf_mem_copy(default_key, &crypto_key->keyval, crypto_key->keylen);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_set_wep_key(struct wlan_mlme_wep_cfg *wep_params,
			    enum wep_key_id wep_keyid, uint8_t *key_to_set,
			    qdf_size_t len)
{
	if (len == 0)
		return QDF_STATUS_E_FAILURE;

	mlme_legacy_debug("WEP set key for key_id:%d key_len:%zd",
			  wep_keyid, len);
	switch (wep_keyid) {
	case MLME_WEP_DEFAULT_KEY_1:
		wlan_mlme_set_cfg_str(key_to_set,
				      &wep_params->wep_default_key_1,
				      len);
		break;

	case MLME_WEP_DEFAULT_KEY_2:
		wlan_mlme_set_cfg_str(key_to_set,
				      &wep_params->wep_default_key_2,
				      len);
		break;

	case MLME_WEP_DEFAULT_KEY_3:
		wlan_mlme_set_cfg_str(key_to_set,
				      &wep_params->wep_default_key_3,
				      len);
		break;

	case MLME_WEP_DEFAULT_KEY_4:
		wlan_mlme_set_cfg_str(key_to_set,
				      &wep_params->wep_default_key_4,
				      len);
		break;

	default:
		mlme_legacy_err("Invalid key id:%d", wep_keyid);
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_is_11h_enabled(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.gen.enabled_11h;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_set_11h_enabled(struct wlan_objmgr_psoc *psoc, bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.gen.enabled_11h = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_is_11d_enabled(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.gen.enabled_11d;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_set_11d_enabled(struct wlan_objmgr_psoc *psoc, bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.gen.enabled_11d = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_is_rf_test_mode_enabled(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.gen.enabled_rf_test_mode;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_set_rf_test_mode_enabled(struct wlan_objmgr_psoc *psoc, bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.gen.enabled_rf_test_mode = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_cfg_set_vht_chan_width(struct wlan_objmgr_psoc *psoc, uint8_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.vht_caps.vht_cap_info.supp_chan_width = value;
	if (value == VHT_CAP_160_AND_80P80_SUPP ||
	    value == VHT_CAP_160_SUPP) {
		mlme_obj->cfg.vht_caps.vht_cap_info.vht_extended_nss_bw_cap = 1;
		mlme_obj->cfg.vht_caps.vht_cap_info.extended_nss_bw_supp = 0;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_cfg_get_vht_chan_width(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.supp_chan_width;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_cfg_set_vht_ldpc_coding_cap(struct wlan_objmgr_psoc *psoc,
						 bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.vht_caps.vht_cap_info.ldpc_coding_cap = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_cfg_set_short_gi_160_mhz(struct wlan_objmgr_psoc *psoc,
				   bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.vht_caps.vht_cap_info.short_gi_160mhz = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_cfg_get_short_gi_160_mhz(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.short_gi_160mhz;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_cfg_get_vht_tx_stbc(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.tx_stbc;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_cfg_get_vht_rx_stbc(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.rx_stbc;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_cfg_set_vht_tx_bfee_ant_supp(struct wlan_objmgr_psoc *psoc,
					uint8_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.vht_caps.vht_cap_info.tx_bfee_ant_supp = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_cfg_get_vht_tx_bfee_ant_supp(struct wlan_objmgr_psoc *psoc,
					uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.tx_bfee_ant_supp;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_cfg_get_vht_rx_mcs_map(struct wlan_objmgr_psoc *psoc,
					    uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.rx_mcs_map;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_cfg_set_vht_rx_mcs_map(struct wlan_objmgr_psoc *psoc,
					    uint32_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.vht_caps.vht_cap_info.rx_mcs_map = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_cfg_get_vht_tx_mcs_map(struct wlan_objmgr_psoc *psoc,
					    uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.tx_mcs_map;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_cfg_set_vht_tx_mcs_map(struct wlan_objmgr_psoc *psoc,
					    uint32_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.vht_caps.vht_cap_info.tx_mcs_map = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_cfg_set_vht_rx_supp_data_rate(struct wlan_objmgr_psoc *psoc,
					uint32_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.vht_caps.vht_cap_info.rx_supp_data_rate = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_cfg_set_vht_tx_supp_data_rate(struct wlan_objmgr_psoc *psoc,
					uint32_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.vht_caps.vht_cap_info.tx_supp_data_rate = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_cfg_get_vht_basic_mcs_set(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.basic_mcs_set;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_cfg_set_vht_basic_mcs_set(struct wlan_objmgr_psoc *psoc,
				    uint32_t value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.vht_caps.vht_cap_info.basic_mcs_set = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_vht_enable_tx_bf(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.su_bformee;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_vht_tx_su_beamformer(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.su_bformer;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_vht_channel_width(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.channel_width;

	return QDF_STATUS_SUCCESS;
}


QDF_STATUS
wlan_mlme_get_vht_rx_mcs_8_9(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.rx_mcs;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_vht_tx_mcs_8_9(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.tx_mcs;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_vht_rx_mcs_2x2(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.rx_mcs2x2;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_vht_tx_mcs_2x2(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.tx_mcs2x2;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_vht20_mcs9(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.enable_vht20_mcs9;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_indoor_support_for_nan(struct wlan_objmgr_psoc *psoc,
				     bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = false;
		mlme_legacy_err("Failed to get MLME Obj");
		return QDF_STATUS_E_INVAL;
	}

	*value = mlme_obj->cfg.reg.enable_nan_on_indoor_channels;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_srd_master_mode_for_vdev(struct wlan_objmgr_psoc *psoc,
				       enum QDF_OPMODE vdev_opmode,
				       bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = false;
		mlme_legacy_err("Failed to get MLME Obj");
		return QDF_STATUS_E_INVAL;
	}

	switch (vdev_opmode) {
	case QDF_SAP_MODE:
		*value = mlme_obj->cfg.reg.etsi_srd_chan_in_master_mode &
			 MLME_SRD_MASTER_MODE_SAP;
		break;
	case QDF_P2P_GO_MODE:
		*value = mlme_obj->cfg.reg.etsi_srd_chan_in_master_mode &
			 MLME_SRD_MASTER_MODE_P2P_GO;
		break;
	case QDF_NAN_DISC_MODE:
		*value = mlme_obj->cfg.reg.etsi_srd_chan_in_master_mode &
			 MLME_SRD_MASTER_MODE_NAN;
		break;
	default:
		mlme_legacy_err("Unexpected opmode %d", vdev_opmode);
		*value = false;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_enable_dynamic_nss_chains_cfg(struct wlan_objmgr_psoc *psoc,
					    bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.nss_chains_ini_cfg.enable_dynamic_nss_chains_cfg;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_vht_enable2x2(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.enable2x2;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_force_sap_enabled(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.acs.force_sap_start;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_set_vht_enable2x2(struct wlan_objmgr_psoc *psoc, bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.vht_caps.vht_cap_info.enable2x2 = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_vht_enable_paid(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.enable_paid;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_vht_enable_gid(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.enable_gid;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_vht_for_24ghz(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.b24ghz_band;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_set_vht_for_24ghz(struct wlan_objmgr_psoc *psoc, bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.vht_caps.vht_cap_info.b24ghz_band = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_vendor_vht_for_24ghz(struct wlan_objmgr_psoc *psoc, bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.vht_caps.vht_cap_info.vendor_24ghz_band;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
mlme_update_vht_cap(struct wlan_objmgr_psoc *psoc, struct wma_tgt_vht_cap *cfg)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	struct mlme_vht_capabilities_info *vht_cap_info;
	uint32_t value = 0;
	bool hw_rx_ldpc_enabled;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	vht_cap_info = &mlme_obj->cfg.vht_caps.vht_cap_info;

	/*
	 * VHT max MPDU length:
	 * override if user configured value is too high
	 * that the target cannot support
	 */
	if (vht_cap_info->ampdu_len > cfg->vht_max_mpdu)
		vht_cap_info->ampdu_len = cfg->vht_max_mpdu;
	if (vht_cap_info->ampdu_len >= 1)
		mlme_obj->cfg.ht_caps.ht_cap_info.maximal_amsdu_size = 1;
	value = (CFG_VHT_BASIC_MCS_SET_STADEF & VHT_MCS_1x1) |
		vht_cap_info->basic_mcs_set;
	if (vht_cap_info->enable2x2)
		value = (value & VHT_MCS_2x2) | (vht_cap_info->rx_mcs2x2 << 2);
	vht_cap_info->basic_mcs_set = value;

	value = (CFG_VHT_RX_MCS_MAP_STADEF & VHT_MCS_1x1) |
		 vht_cap_info->rx_mcs;

	if (vht_cap_info->enable2x2)
		value = (value & VHT_MCS_2x2) | (vht_cap_info->rx_mcs2x2 << 2);
	vht_cap_info->rx_mcs_map = value;

	value = (CFG_VHT_TX_MCS_MAP_STADEF & VHT_MCS_1x1) |
		 vht_cap_info->tx_mcs;
	if (vht_cap_info->enable2x2)
		value = (value & VHT_MCS_2x2) | (vht_cap_info->tx_mcs2x2 << 2);
	vht_cap_info->tx_mcs_map = value;

	 /* Set HW RX LDPC capability */
	hw_rx_ldpc_enabled = !!cfg->vht_rx_ldpc;
	if (vht_cap_info->ldpc_coding_cap && !hw_rx_ldpc_enabled)
		vht_cap_info->ldpc_coding_cap = hw_rx_ldpc_enabled;

	/* set the Guard interval 80MHz */
	if (vht_cap_info->short_gi_80mhz && !cfg->vht_short_gi_80)
		vht_cap_info->short_gi_80mhz = cfg->vht_short_gi_80;

	/* Set VHT TX STBC cap */
	if (vht_cap_info->tx_stbc && !cfg->vht_tx_stbc)
		vht_cap_info->tx_stbc = cfg->vht_tx_stbc;

	/* Set VHT RX STBC cap */
	if (vht_cap_info->rx_stbc && !cfg->vht_rx_stbc)
		vht_cap_info->rx_stbc = cfg->vht_rx_stbc;

	/* Set VHT SU Beamformer cap */
	if (vht_cap_info->su_bformer && !cfg->vht_su_bformer)
		vht_cap_info->su_bformer = cfg->vht_su_bformer;

	/* check and update SU BEAMFORMEE capabality */
	if (vht_cap_info->su_bformee && !cfg->vht_su_bformee)
		vht_cap_info->su_bformee = cfg->vht_su_bformee;

	/* Set VHT MU Beamformer cap */
	if (vht_cap_info->mu_bformer && !cfg->vht_mu_bformer)
		vht_cap_info->mu_bformer = cfg->vht_mu_bformer;

	/* Set VHT MU Beamformee cap */
	if (vht_cap_info->enable_mu_bformee && !cfg->vht_mu_bformee)
		vht_cap_info->enable_mu_bformee = cfg->vht_mu_bformee;

	/*
	 * VHT max AMPDU len exp:
	 * override if user configured value is too high
	 * that the target cannot support.
	 * Even though Rome publish ampdu_len=7, it can
	 * only support 4 because of some h/w bug.
	 */
	if (vht_cap_info->ampdu_len_exponent > cfg->vht_max_ampdu_len_exp)
		vht_cap_info->ampdu_len_exponent = cfg->vht_max_ampdu_len_exp;

	/* Set VHT TXOP PS CAP */
	if (vht_cap_info->txop_ps && !cfg->vht_txop_ps)
		vht_cap_info->txop_ps = cfg->vht_txop_ps;

	/* set the Guard interval 160MHz */
	if (vht_cap_info->short_gi_160mhz && !cfg->vht_short_gi_160)
		vht_cap_info->short_gi_160mhz = cfg->vht_short_gi_160;

	vht_cap_info->vht_mcs_10_11_supp = cfg->vht_mcs_10_11_supp;
	mlme_legacy_debug(" vht_mcs_10_11_supp %d", cfg->vht_mcs_10_11_supp);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_update_nss_vht_cap(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	struct mlme_vht_capabilities_info *vht_cap_info;
	uint32_t temp = 0;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	vht_cap_info = &mlme_obj->cfg.vht_caps.vht_cap_info;

	temp = vht_cap_info->basic_mcs_set;
	temp = (temp & 0xFFFC) | vht_cap_info->rx_mcs;
	if (vht_cap_info->enable2x2)
		temp = (temp & 0xFFF3) | (vht_cap_info->rx_mcs2x2 << 2);
	else
		temp |= 0x000C;

	vht_cap_info->basic_mcs_set = temp;

	temp = vht_cap_info->rx_mcs_map;
	temp = (temp & 0xFFFC) | vht_cap_info->rx_mcs;
	if (vht_cap_info->enable2x2)
		temp = (temp & 0xFFF3) | (vht_cap_info->rx_mcs2x2 << 2);
	else
		temp |= 0x000C;

	vht_cap_info->rx_mcs_map = temp;

	temp = vht_cap_info->tx_mcs_map;
	temp = (temp & 0xFFFC) | vht_cap_info->tx_mcs;
	if (vht_cap_info->enable2x2)
		temp = (temp & 0xFFF3) | (vht_cap_info->tx_mcs2x2 << 2);
	else
		temp |= 0x000C;

	vht_cap_info->tx_mcs_map = temp;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_is_sap_uapsd_enabled(struct wlan_objmgr_psoc *psoc,
					  bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.qos_mlme_params.sap_uapsd_enabled;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_dtim_selection_diversity(struct wlan_objmgr_psoc *psoc,
						  uint32_t *dtim_selection_div)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*dtim_selection_div = cfg_default(CFG_DTIM_SELECTION_DIVERSITY);
		return QDF_STATUS_E_FAILURE;
	}

	*dtim_selection_div = mlme_obj->cfg.ps_params.dtim_selection_diversity;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_bmps_min_listen_interval(struct wlan_objmgr_psoc *psoc,
						  uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_BMPS_MINIMUM_LI);
		return QDF_STATUS_E_FAILURE;
	}

	*value = mlme_obj->cfg.ps_params.bmps_min_listen_interval;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_bmps_max_listen_interval(struct wlan_objmgr_psoc *psoc,
						  uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_BMPS_MAXIMUM_LI);
		return QDF_STATUS_E_FAILURE;
	}

	*value = mlme_obj->cfg.ps_params.bmps_max_listen_interval;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_set_sap_uapsd_flag(struct wlan_objmgr_psoc *psoc,
					bool value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.qos_mlme_params.sap_uapsd_enabled &= value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_rrm_enabled(struct wlan_objmgr_psoc *psoc,
				     bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*value = mlme_obj->cfg.rrm_config.rrm_enabled;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_get_auto_bmps_timer_value(struct wlan_objmgr_psoc *psoc,
					       uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_AUTO_BMPS_ENABLE_TIMER);
		return QDF_STATUS_E_FAILURE;
	}

	*value = mlme_obj->cfg.ps_params.auto_bmps_timer_val;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_is_bmps_enabled(struct wlan_objmgr_psoc *psoc,
				     bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_ENABLE_PS);
		return QDF_STATUS_E_FAILURE;
	}

	*value = mlme_obj->cfg.ps_params.is_bmps_enabled;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_is_imps_enabled(struct wlan_objmgr_psoc *psoc,
				     bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_ENABLE_IMPS);
		return QDF_STATUS_E_FAILURE;
	}

	*value = mlme_obj->cfg.ps_params.is_imps_enabled;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_override_bmps_imps(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.ps_params.is_imps_enabled = 0;
	mlme_obj->cfg.ps_params.is_bmps_enabled = 0;

	return QDF_STATUS_SUCCESS;
}

void wlan_mlme_get_wps_uuid(struct wlan_mlme_wps_params *wps_params,
			    uint8_t *data)
{
	qdf_size_t len = wps_params->wps_uuid.len;

	wlan_mlme_get_cfg_str(data, &wps_params->wps_uuid, &len);
}

QDF_STATUS
wlan_mlme_get_self_gen_frm_pwr(struct wlan_objmgr_psoc *psoc,
			       uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_SELF_GEN_FRM_PWR);
		mlme_legacy_err("Failed to get MLME Obj");
		return QDF_STATUS_E_FAILURE;
	}

	*value = mlme_obj->cfg.reg.self_gen_frm_pwr;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_4way_hs_offload(struct wlan_objmgr_psoc *psoc, uint32_t *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_DISABLE_4WAY_HS_OFFLOAD);
		mlme_legacy_err("Failed to get MLME Obj");
		return QDF_STATUS_E_FAILURE;
	}

	*value = mlme_obj->cfg.gen.disable_4way_hs_offload;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_bmiss_skip_full_scan_value(struct wlan_objmgr_psoc *psoc,
					 bool *value)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*value = cfg_default(CFG_BMISS_SKIP_FULL_SCAN);
		mlme_legacy_err("Failed to get MLME Obj");
		return QDF_STATUS_E_FAILURE;
	}

	*value = mlme_obj->cfg.gen.bmiss_skip_full_scan;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_get_peer_phymode(struct wlan_objmgr_psoc *psoc, uint8_t *mac,
				 enum wlan_phymode *peer_phymode)
{
	struct wlan_objmgr_peer *peer;

	peer = wlan_objmgr_get_peer_by_mac(psoc, mac, WLAN_MLME_NB_ID);
	if (!peer) {
		mlme_legacy_err("peer object is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	*peer_phymode = wlan_peer_get_phymode(peer);
	wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_set_tgt_wpa3_roam_cap(struct wlan_objmgr_psoc *psoc,
				      uint32_t akm_bitmap)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.lfr.fw_akm_bitmap |= akm_bitmap;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_ignore_fw_reg_offload_ind(struct wlan_objmgr_psoc *psoc,
					bool *disabled)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	*disabled = mlme_obj->cfg.reg.ignore_fw_reg_offload_ind;
	return QDF_STATUS_SUCCESS;
}

char *mlme_get_roam_status_str(uint32_t roam_status)
{
	switch (roam_status) {
	case 0:
		return "SUCCESS";
	case 1:
		return "FAILED";
	case 2:
		return "NO ROAM";
	default:
		return "UNKNOWN";
	}
}

char *mlme_get_roam_scan_type_str(uint32_t roam_scan_type)
{
	switch (roam_scan_type) {
	case 0:
		return "PARTIAL";
	case 1:
		return "FULL";
	case 2:
		return "NO SCAN";
	case 3:
		return "Higher Band";
	default:
		return "UNKNOWN";
	}
}

char *mlme_get_roam_trigger_str(uint32_t roam_scan_trigger)
{
	switch (roam_scan_trigger) {
	case WMI_ROAM_TRIGGER_REASON_PER:
		return "PER";
	case WMI_ROAM_TRIGGER_REASON_BMISS:
		return "BEACON MISS";
	case WMI_ROAM_TRIGGER_REASON_LOW_RSSI:
		return "LOW RSSI";
	case WMI_ROAM_TRIGGER_REASON_HIGH_RSSI:
		return "HIGH RSSI";
	case WMI_ROAM_TRIGGER_REASON_PERIODIC:
		return "PERIODIC SCAN";
	case WMI_ROAM_TRIGGER_REASON_MAWC:
		return "MAWC";
	case WMI_ROAM_TRIGGER_REASON_DENSE:
		return "DENSE ENVIRONMENT";
	case WMI_ROAM_TRIGGER_REASON_BACKGROUND:
		return "BACKGROUND SCAN";
	case WMI_ROAM_TRIGGER_REASON_FORCED:
		return "FORCED SCAN";
	case WMI_ROAM_TRIGGER_REASON_BTM:
		return "BTM TRIGGER";
	case WMI_ROAM_TRIGGER_REASON_UNIT_TEST:
		return "TEST COMMMAND";
	case WMI_ROAM_TRIGGER_REASON_BSS_LOAD:
		return "HIGH BSS LOAD";
	case WMI_ROAM_TRIGGER_REASON_DEAUTH:
		return "DEAUTH RECEIVED";
	case WMI_ROAM_TRIGGER_REASON_IDLE:
		return "IDLE STATE SCAN";
	case WMI_ROAM_TRIGGER_REASON_STA_KICKOUT:
		return "STA KICKOUT";
	case WMI_ROAM_TRIGGER_REASON_ESS_RSSI:
		return "ESS RSSI";
	case WMI_ROAM_TRIGGER_REASON_WTC_BTM:
		return "WTC BTM";
	case WMI_ROAM_TRIGGER_REASON_NONE:
		return "NONE";
	case WMI_ROAM_TRIGGER_REASON_PMK_TIMEOUT:
		return "PMK Expired";
	default:
		return "UNKNOWN";
	}
}

void mlme_get_converted_timestamp(uint32_t timestamp, char *time)
{
	uint32_t hr, mins, secs;

	secs = timestamp / 1000;
	mins = secs / 60;
	hr = mins / 60;
	qdf_snprint(time, TIME_STRING_LEN, "[%02d:%02d:%02d.%06u]",
		    (hr % 24), (mins % 60), (secs % 60),
		    (timestamp % 1000) * 1000);
}

#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
void wlan_mlme_set_sae_single_pmk_bss_cap(struct wlan_objmgr_psoc *psoc,
					  uint8_t vdev_id, bool val)
{
	struct mlme_legacy_priv *mlme_priv;
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_OBJMGR_ID);

	if (!vdev) {
		mlme_err("get vdev failed");
		return;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);
		mlme_legacy_err("vdev legacy private object is NULL");
		return;
	}

	mlme_priv->mlme_roam.sae_single_pmk.sae_single_pmk_ap = val;

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);
}

void wlan_mlme_update_sae_single_pmk(struct wlan_objmgr_vdev *vdev,
				     struct mlme_pmk_info *sae_single_pmk)
{
	struct mlme_legacy_priv *mlme_priv;
	int32_t keymgmt;
	bool is_sae_connection = false;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return;
	}

	keymgmt = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_KEY_MGMT);
	if (keymgmt < 0) {
		mlme_legacy_err("Invalid mgmt cipher");
		return;
	}

	if (keymgmt & (1 << WLAN_CRYPTO_KEY_MGMT_SAE))
		is_sae_connection = true;

	mlme_legacy_debug("SAE_SPMK: single_pmk_ap:%d, is_sae_connection:%d, pmk_len:%d",
			  mlme_priv->mlme_roam.sae_single_pmk.sae_single_pmk_ap,
			  is_sae_connection, sae_single_pmk->pmk_len);

	if (mlme_priv->mlme_roam.sae_single_pmk.sae_single_pmk_ap &&
	    is_sae_connection)
		mlme_priv->mlme_roam.sae_single_pmk.pmk_info = *sae_single_pmk;
}

void wlan_mlme_get_sae_single_pmk_info(struct wlan_objmgr_vdev *vdev,
				       struct wlan_mlme_sae_single_pmk *pmksa)
{
	struct mlme_legacy_priv *mlme_priv;
	struct mlme_pmk_info *pmk_info;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return;
	}

	pmk_info = &mlme_priv->mlme_roam.sae_single_pmk.pmk_info;

	pmksa->sae_single_pmk_ap =
		mlme_priv->mlme_roam.sae_single_pmk.sae_single_pmk_ap;
	pmksa->pmk_info.spmk_timeout_period = pmk_info->spmk_timeout_period;
	pmksa->pmk_info.spmk_timestamp = pmk_info->spmk_timestamp;

	if (pmk_info->pmk_len) {
		qdf_mem_copy(pmksa->pmk_info.pmk, pmk_info->pmk,
			     pmk_info->pmk_len);
		pmksa->pmk_info.pmk_len = pmk_info->pmk_len;
		return;
	}

	qdf_mem_zero(pmksa->pmk_info.pmk, sizeof(*pmksa->pmk_info.pmk));
	pmksa->pmk_info.pmk_len = 0;
}

void wlan_mlme_clear_sae_single_pmk_info(struct wlan_objmgr_vdev *vdev,
					 struct mlme_pmk_info *pmk_recv)
{
	struct mlme_legacy_priv *mlme_priv;
	struct wlan_mlme_sae_single_pmk *sae_single_pmk;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return;
	}

	sae_single_pmk = &mlme_priv->mlme_roam.sae_single_pmk;

	if (!pmk_recv) {
		/* Process flush pmk cmd */
		mlme_legacy_debug("Flush sae_single_pmk info");
		qdf_mem_zero(&sae_single_pmk->pmk_info,
			     sizeof(sae_single_pmk->pmk_info));
	} else if (pmk_recv->pmk_len != sae_single_pmk->pmk_info.pmk_len) {
		mlme_legacy_debug("Invalid pmk len");
		return;
	} else if (!qdf_mem_cmp(&sae_single_pmk->pmk_info.pmk, pmk_recv->pmk,
		   pmk_recv->pmk_len)) {
			/* Process delete pmk cmd */
			mlme_legacy_debug("Clear sae_single_pmk info");
			qdf_mem_zero(&sae_single_pmk->pmk_info,
				     sizeof(sae_single_pmk->pmk_info));
	}
}
#endif

char *mlme_get_roam_fail_reason_str(uint32_t result)
{
	switch (result) {
	case WMI_ROAM_FAIL_REASON_NO_SCAN_START:
		return "SCAN NOT STARTED";
	case WMI_ROAM_FAIL_REASON_NO_AP_FOUND:
		return "NO AP FOUND";
	case WMI_ROAM_FAIL_REASON_NO_CAND_AP_FOUND:
		return "NO CANDIDATE FOUND";
	case WMI_ROAM_FAIL_REASON_HOST:
		return "HOST ABORTED";
	case WMI_ROAM_FAIL_REASON_AUTH_SEND:
		return "Send AUTH Failed";
	case WMI_ROAM_FAIL_REASON_AUTH_RECV:
		return "Received AUTH with FAILURE Status";
	case WMI_ROAM_FAIL_REASON_NO_AUTH_RESP:
		return "No Auth response from AP";
	case WMI_ROAM_FAIL_REASON_REASSOC_SEND:
		return "Send Re-assoc request failed";
	case WMI_ROAM_FAIL_REASON_REASSOC_RECV:
		return "Received Re-Assoc resp with Failure status";
	case WMI_ROAM_FAIL_REASON_NO_REASSOC_RESP:
		return "No Re-assoc response from AP";
	case WMI_ROAM_FAIL_REASON_EAPOL_M1_TIMEOUT:
		return "EAPOL M1 timed out";
	case WMI_ROAM_FAIL_REASON_MLME:
		return "MLME error";
	case WMI_ROAM_FAIL_REASON_INTERNAL_ABORT:
		return "Fw aborted roam";
	case WMI_ROAM_FAIL_REASON_SCAN_START:
		return "Unable to start roam scan";
	case WMI_ROAM_FAIL_REASON_AUTH_NO_ACK:
		return "No ACK for Auth req";
	case WMI_ROAM_FAIL_REASON_AUTH_INTERNAL_DROP:
		return "Auth req dropped internally";
	case WMI_ROAM_FAIL_REASON_REASSOC_NO_ACK:
		return "No ACK for Re-assoc req";
	case WMI_ROAM_FAIL_REASON_REASSOC_INTERNAL_DROP:
		return "Re-assoc dropped internally";
	case WMI_ROAM_FAIL_REASON_EAPOL_M2_SEND:
		return "Unable to send M2 frame";
	case WMI_ROAM_FAIL_REASON_EAPOL_M2_INTERNAL_DROP:
		return "M2 Frame dropped internally";
	case WMI_ROAM_FAIL_REASON_EAPOL_M2_NO_ACK:
		return "No ACK for M2 frame";
	case WMI_ROAM_FAIL_REASON_EAPOL_M3_TIMEOUT:
		return "EAPOL M3 timed out";
	case WMI_ROAM_FAIL_REASON_EAPOL_M4_SEND:
		return "Unable to send M4 frame";
	case WMI_ROAM_FAIL_REASON_EAPOL_M4_INTERNAL_DROP:
		return "M4 frame dropped internally";
	case WMI_ROAM_FAIL_REASON_EAPOL_M4_NO_ACK:
		return "No ACK for M4 frame";
	case WMI_ROAM_FAIL_REASON_NO_SCAN_FOR_FINAL_BMISS:
		return "No scan on final BMISS";
	case WMI_ROAM_FAIL_REASON_DISCONNECT:
		return "Disconnect received during handoff";
	case WMI_ROAM_FAIL_REASON_SYNC:
		return "Previous roam sync pending";
	case WMI_ROAM_FAIL_REASON_SAE_INVALID_PMKID:
		return "Reason assoc reject - invalid PMKID";
	case WMI_ROAM_FAIL_REASON_SAE_PREAUTH_TIMEOUT:
		return "SAE preauth timed out";
	case WMI_ROAM_FAIL_REASON_SAE_PREAUTH_FAIL:
		return "SAE preauth failed";
	case WMI_ROAM_FAIL_REASON_UNABLE_TO_START_ROAM_HO:
		return "Start handoff failed- internal error";
	default:
		return "UNKNOWN";
	}
}

char *mlme_get_sub_reason_str(uint32_t sub_reason)
{
	switch (sub_reason) {
	case WMI_ROAM_TRIGGER_SUB_REASON_PERIODIC_TIMER:
		return "PERIODIC TIMER";
	case WMI_ROAM_TRIGGER_SUB_REASON_LOW_RSSI_PERIODIC:
		return "LOW RSSI PERIODIC TIMER1";
	case WMI_ROAM_TRIGGER_SUB_REASON_BTM_DI_TIMER:
		return "BTM DISASSOC IMMINENT TIMER";
	case WMI_ROAM_TRIGGER_SUB_REASON_FULL_SCAN:
		return "FULL SCAN";
	case WMI_ROAM_TRIGGER_SUB_REASON_CU_PERIODIC:
		return "CU PERIODIC Timer1";
	case WMI_ROAM_TRIGGER_SUB_REASON_INACTIVITY_TIMER_LOW_RSSI:
		return "LOW RSSI INACTIVE TIMER";
	case WMI_ROAM_TRIGGER_SUB_REASON_PERIODIC_TIMER_AFTER_INACTIVITY_CU:
		return "CU PERIODIC TIMER2";
	case WMI_ROAM_TRIGGER_SUB_REASON_PERIODIC_TIMER_AFTER_INACTIVITY_LOW_RSSI:
		return "LOW RSSI PERIODIC TIMER2";
	case WMI_ROAM_TRIGGER_SUB_REASON_INACTIVITY_TIMER_CU:
		return "CU INACTIVITY TIMER";
	default:
		return "NONE";
	}
}

QDF_STATUS
wlan_mlme_get_mgmt_max_retry(struct wlan_objmgr_psoc *psoc,
			     uint8_t *max_retry)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj) {
		*max_retry = cfg_default(CFG_MGMT_RETRY_MAX);
		return QDF_STATUS_E_FAILURE;
	}

	*max_retry = mlme_obj->cfg.gen.mgmt_retry_max;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_status_ring_buffer(struct wlan_objmgr_psoc *psoc,
				 bool *enable_ring_buffer)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj) {
		*enable_ring_buffer = cfg_default(CFG_ENABLE_RING_BUFFER);
		return QDF_STATUS_E_FAILURE;
	}

	*enable_ring_buffer = mlme_obj->cfg.gen.enable_ring_buffer;
	return QDF_STATUS_SUCCESS;
}

bool wlan_mlme_get_peer_unmap_conf(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return false;

	return mlme_obj->cfg.gen.enable_peer_unmap_conf_support;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
QDF_STATUS
wlan_mlme_get_roam_reason_vsie_status(struct wlan_objmgr_psoc *psoc,
				      uint8_t *roam_reason_vsie_enable)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*roam_reason_vsie_enable =
			cfg_default(CFG_ENABLE_ROAM_REASON_VSIE);
		return QDF_STATUS_E_FAILURE;
	}

	*roam_reason_vsie_enable = mlme_obj->cfg.lfr.enable_roam_reason_vsie;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_set_roam_reason_vsie_status(struct wlan_objmgr_psoc *psoc,
				      uint8_t roam_reason_vsie_enable)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.lfr.enable_roam_reason_vsie = roam_reason_vsie_enable;
	return QDF_STATUS_SUCCESS;
}

uint32_t wlan_mlme_get_roaming_triggers(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return cfg_default(CFG_ROAM_TRIGGER_BITMAP);

	return mlme_obj->cfg.lfr.roam_trigger_bitmap;
}

QDF_STATUS
wlan_mlme_get_roaming_offload(struct wlan_objmgr_psoc *psoc,
			      bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR3_ROAMING_OFFLOAD);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.lfr3_roaming_offload;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_enable_disconnect_roam_offload(struct wlan_objmgr_psoc *psoc,
					     bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_ENABLE_DISCONNECT_ROAM);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.enable_disconnect_roam_offload;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_enable_idle_roam(struct wlan_objmgr_psoc *psoc, bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_ENABLE_IDLE_ROAM);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.enable_idle_roam;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_idle_roam_rssi_delta(struct wlan_objmgr_psoc *psoc, uint32_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_IDLE_ROAM_RSSI_DELTA);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.idle_roam_rssi_delta;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_idle_roam_inactive_time(struct wlan_objmgr_psoc *psoc,
				      uint32_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_IDLE_ROAM_INACTIVE_TIME);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.idle_roam_inactive_time;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_idle_data_packet_count(struct wlan_objmgr_psoc *psoc,
				     uint32_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_IDLE_ROAM_PACKET_COUNT);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.idle_data_packet_count;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_idle_roam_min_rssi(struct wlan_objmgr_psoc *psoc, uint32_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_IDLE_ROAM_MIN_RSSI);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.idle_roam_min_rssi;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_idle_roam_band(struct wlan_objmgr_psoc *psoc, uint32_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_IDLE_ROAM_BAND);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.idle_roam_band;

	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS
wlan_mlme_get_dfs_chan_ageout_time(struct wlan_objmgr_psoc *psoc,
				   uint8_t *dfs_chan_ageout_time)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj) {
		*dfs_chan_ageout_time =
			cfg_default(CFG_DFS_CHAN_AGEOUT_TIME);
		return QDF_STATUS_E_FAILURE;
	}

	*dfs_chan_ageout_time = mlme_obj->cfg.gen.dfs_chan_ageout_time;

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_SAE

#define NUM_RETRY_BITS 3
#define ROAM_AUTH_INDEX 2
#define ASSOC_INDEX 1
#define AUTH_INDEX 0
#define MAX_RETRIES 2
#define MAX_ROAM_AUTH_RETRIES 1
#define MAX_AUTH_RETRIES 3

QDF_STATUS
wlan_mlme_get_sae_assoc_retry_count(struct wlan_objmgr_psoc *psoc,
				    uint8_t *retry_count)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj) {
		*retry_count = 0;
		return QDF_STATUS_E_FAILURE;
	}

	*retry_count =
		QDF_GET_BITS(mlme_obj->cfg.gen.sae_connect_retries,
			     ASSOC_INDEX * NUM_RETRY_BITS, NUM_RETRY_BITS);

	*retry_count = QDF_MIN(MAX_RETRIES, *retry_count);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_sae_auth_retry_count(struct wlan_objmgr_psoc *psoc,
				   uint8_t *retry_count)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj) {
		*retry_count = 0;
		return QDF_STATUS_E_FAILURE;
	}

	*retry_count =
		QDF_GET_BITS(mlme_obj->cfg.gen.sae_connect_retries,
			     AUTH_INDEX * NUM_RETRY_BITS, NUM_RETRY_BITS);

	*retry_count = QDF_MIN(MAX_AUTH_RETRIES, *retry_count);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_sae_roam_auth_retry_count(struct wlan_objmgr_psoc *psoc,
					uint8_t *retry_count)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj) {
		*retry_count = 0;
		return QDF_STATUS_E_FAILURE;
	}

	*retry_count =
		QDF_GET_BITS(mlme_obj->cfg.gen.sae_connect_retries,
			     ROAM_AUTH_INDEX * NUM_RETRY_BITS, NUM_RETRY_BITS);

	*retry_count = QDF_MIN(MAX_ROAM_AUTH_RETRIES, *retry_count);

	return QDF_STATUS_SUCCESS;
}

#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
bool
wlan_mlme_get_dual_sta_roaming_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	bool dual_sta_roaming_enabled;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return cfg_default(CFG_ENABLE_DUAL_STA_ROAM_OFFLOAD);

	dual_sta_roaming_enabled =
			mlme_obj->cfg.lfr.lfr3_roaming_offload &&
			mlme_obj->cfg.lfr.lfr3_dual_sta_roaming_enabled &&
			wlan_mlme_get_dual_sta_roam_support(psoc) &&
			policy_mgr_is_hw_dbs_capable(psoc);

	return dual_sta_roaming_enabled;
}
#endif

QDF_STATUS
wlan_mlme_get_roam_scan_offload_enabled(struct wlan_objmgr_psoc *psoc,
					bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_ROAM_SCAN_OFFLOAD_ENABLED);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.roam_scan_offload_enabled;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_roam_bmiss_final_bcnt(struct wlan_objmgr_psoc *psoc,
				    uint8_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_ROAM_BMISS_FINAL_BCNT);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.roam_bmiss_final_bcnt;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_roam_bmiss_first_bcnt(struct wlan_objmgr_psoc *psoc,
				    uint8_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_ROAM_BMISS_FIRST_BCNT);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.roam_bmiss_first_bcnt;

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_ADAPTIVE_11R
bool wlan_mlme_adaptive_11r_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return cfg_default(CFG_ADAPTIVE_11R);

	return mlme_obj->cfg.lfr.enable_adaptive_11r;
}
#endif

QDF_STATUS
wlan_mlme_get_mawc_enabled(struct wlan_objmgr_psoc *psoc, bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_MAWC_FEATURE_ENABLED);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.mawc_enabled;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_mawc_roam_enabled(struct wlan_objmgr_psoc *psoc, bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_MAWC_ROAM_ENABLED);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.mawc_roam_enabled;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_mawc_roam_traffic_threshold(struct wlan_objmgr_psoc *psoc,
					  uint32_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_MAWC_ROAM_TRAFFIC_THRESHOLD);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.mawc_roam_traffic_threshold;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_mawc_roam_ap_rssi_threshold(struct wlan_objmgr_psoc *psoc,
					  uint32_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_MAWC_ROAM_AP_RSSI_THRESHOLD);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.mawc_roam_ap_rssi_threshold;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_mawc_roam_rssi_high_adjust(struct wlan_objmgr_psoc *psoc,
					 uint8_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_MAWC_ROAM_RSSI_HIGH_ADJUST);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.mawc_roam_rssi_high_adjust;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_mawc_roam_rssi_low_adjust(struct wlan_objmgr_psoc *psoc,
					uint8_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_LFR_MAWC_ROAM_RSSI_LOW_ADJUST);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.mawc_roam_rssi_low_adjust;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_bss_load_enabled(struct wlan_objmgr_psoc *psoc, bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_ENABLE_BSS_LOAD_TRIGGERED_ROAM);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.bss_load_trig.enabled;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_bss_load_threshold(struct wlan_objmgr_psoc *psoc, uint32_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_BSS_LOAD_THRESHOLD);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.bss_load_trig.threshold;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_bss_load_sample_time(struct wlan_objmgr_psoc *psoc,
				   uint32_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_BSS_LOAD_SAMPLE_TIME);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.bss_load_trig.sample_time;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_bss_load_rssi_threshold_5ghz(struct wlan_objmgr_psoc *psoc,
					   int32_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_BSS_LOAD_TRIG_5G_RSSI_THRES);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.bss_load_trig.rssi_threshold_5ghz;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_bss_load_rssi_threshold_24ghz(struct wlan_objmgr_psoc *psoc,
					    int32_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_BSS_LOAD_TRIG_2G_RSSI_THRES);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.lfr.bss_load_trig.rssi_threshold_24ghz;

	return QDF_STATUS_SUCCESS;
}

bool
wlan_mlme_check_chan_param_has_dfs(struct wlan_objmgr_pdev *pdev,
				   struct ch_params *ch_params,
				   uint32_t chan_freq)
{
	bool is_dfs = false;

	if (ch_params->ch_width == CH_WIDTH_160MHZ) {
		is_dfs = true;
	} else if (ch_params->ch_width == CH_WIDTH_80P80MHZ) {
		if (wlan_reg_get_channel_state_for_freq(
			pdev,
			chan_freq) == CHANNEL_STATE_DFS ||
		    wlan_reg_get_channel_state_for_freq(
			pdev,
			ch_params->mhz_freq_seg1) == CHANNEL_STATE_DFS)
			is_dfs = true;
	} else if (wlan_reg_get_channel_state_for_freq(
			pdev, chan_freq) == CHANNEL_STATE_DFS) {
		is_dfs = true;
	}
	if (WLAN_REG_IS_6GHZ_CHAN_FREQ(chan_freq) ||
	    WLAN_REG_IS_24GHZ_CH_FREQ(chan_freq))
		is_dfs = false;

	return is_dfs;
}

QDF_STATUS
wlan_mlme_set_usr_disabled_roaming(struct wlan_objmgr_psoc *psoc, bool val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	mlme_obj->cfg.sta.usr_disabled_roaming = val;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_mlme_get_usr_disabled_roaming(struct wlan_objmgr_psoc *psoc, bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	*val = mlme_obj->cfg.sta.usr_disabled_roaming;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_get_opr_rate(struct wlan_objmgr_vdev *vdev, uint8_t *dst,
			     qdf_size_t *len)
{
	struct mlme_legacy_priv *mlme_priv;

	if (!vdev || !dst || !len) {
		mlme_legacy_err("invalid params");
		return QDF_STATUS_E_INVAL;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (*len < mlme_priv->opr_rate_set.len) {
		mlme_legacy_err("Invalid len %zd, opr_rate len %zd",
				*len, mlme_priv->opr_rate_set.len);
		return QDF_STATUS_E_INVAL;
	}

	*len = mlme_priv->opr_rate_set.len;
	qdf_mem_copy(dst, mlme_priv->opr_rate_set.data, *len);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_set_opr_rate(struct wlan_objmgr_vdev *vdev, uint8_t *src,
			     qdf_size_t len)
{
	struct mlme_legacy_priv *mlme_priv;

	if (!vdev || !src) {
		mlme_legacy_err("invalid params");
		return QDF_STATUS_E_INVAL;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (len > mlme_priv->opr_rate_set.max_len) {
		mlme_legacy_err("Invalid len %zd (>%zd)", len,
				mlme_priv->opr_rate_set.max_len);
		return QDF_STATUS_E_INVAL;
	}

	mlme_priv->opr_rate_set.len = len;
	qdf_mem_copy(mlme_priv->opr_rate_set.data, src, len);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_get_ext_opr_rate(struct wlan_objmgr_vdev *vdev, uint8_t *dst,
			     qdf_size_t *len)
{
	struct mlme_legacy_priv *mlme_priv;

	if (!vdev || !dst || !len) {
		mlme_legacy_err("invalid params");
		return QDF_STATUS_E_INVAL;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (*len < mlme_priv->ext_opr_rate_set.len) {
		mlme_legacy_err("Invalid len %zd, ext_opr_rate len %zd",
				*len, mlme_priv->ext_opr_rate_set.len);
		return QDF_STATUS_E_INVAL;
	}

	*len = mlme_priv->ext_opr_rate_set.len;
	qdf_mem_copy(dst, mlme_priv->ext_opr_rate_set.data, *len);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_set_ext_opr_rate(struct wlan_objmgr_vdev *vdev, uint8_t *src,
			     qdf_size_t len)
{
	struct mlme_legacy_priv *mlme_priv;

	if (!vdev || !src) {
		mlme_legacy_err("invalid params");
		return QDF_STATUS_E_INVAL;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (len > mlme_priv->ext_opr_rate_set.max_len) {
		mlme_legacy_err("Invalid len %zd (>%zd)", len,
				mlme_priv->ext_opr_rate_set.max_len);
		return QDF_STATUS_E_INVAL;
	}

	mlme_priv->ext_opr_rate_set.len = len;
	qdf_mem_copy(mlme_priv->ext_opr_rate_set.data, src, len);

	return QDF_STATUS_SUCCESS;
}

static enum monitor_mode_concurrency
wlan_mlme_get_monitor_mode_concurrency(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return cfg_default(CFG_MONITOR_MODE_CONCURRENCY);

	return mlme_obj->cfg.gen.monitor_mode_concurrency;
}

bool wlan_mlme_is_sta_mon_conc_supported(struct wlan_objmgr_psoc *psoc)
{
	if (wlan_mlme_get_monitor_mode_concurrency(psoc) ==
						MONITOR_MODE_CONC_STA_SCAN_MON)
		return true;

	return false;
}

bool wlan_mlme_is_local_tpe_pref(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return false;

	return mlme_obj->cfg.power.use_local_tpe;
}

bool wlan_mlme_skip_tpe(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return false;

	return mlme_obj->cfg.power.skip_tpe;
}

QDF_STATUS
wlan_mlme_set_ba_2k_jump_iot_ap(struct wlan_objmgr_vdev *vdev, bool found)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv->ba_2k_jump_iot_ap = found;

	return QDF_STATUS_SUCCESS;
}

bool wlan_mlme_is_ba_2k_jump_iot_ap(struct wlan_objmgr_vdev *vdev)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return false;
	}

	return mlme_priv->ba_2k_jump_iot_ap;
}

QDF_STATUS
wlan_mlme_set_bad_htc_he_iot_ap(struct wlan_objmgr_vdev *vdev, bool found)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv->bad_htc_he_iot_ap = found;
	mlme_legacy_debug("set bad htc he iot ap: %d", found);

	return QDF_STATUS_SUCCESS;
}

bool wlan_mlme_is_bad_htc_he_iot_ap(struct wlan_objmgr_vdev *vdev)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return false;
	}

	return mlme_priv->bad_htc_he_iot_ap;
}

QDF_STATUS
wlan_mlme_set_last_delba_sent_time(struct wlan_objmgr_vdev *vdev,
				   qdf_time_t delba_sent_time)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv->last_delba_sent_time = delba_sent_time;

	return QDF_STATUS_SUCCESS;
}

qdf_time_t
wlan_mlme_get_last_delba_sent_time(struct wlan_objmgr_vdev *vdev)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return 0;
	}

	return mlme_priv->last_delba_sent_time;
}

QDF_STATUS mlme_set_user_ps(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			    bool ps_enable)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_OBJMGR_ID);
	if (!vdev)
		return status;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (mlme_priv) {
		mlme_priv->is_usr_ps_enabled = ps_enable;
		status = QDF_STATUS_SUCCESS;
		mlme_legacy_debug("vdev:%d user PS:%d", vdev_id,
				  mlme_priv->is_usr_ps_enabled);
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);

	return status;
}

bool mlme_get_user_ps(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;
	bool usr_ps_enable = false;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_OBJMGR_ID);
	if (!vdev)
		return false;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (mlme_priv)
		usr_ps_enable = mlme_priv->is_usr_ps_enabled;

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_OBJMGR_ID);

	return usr_ps_enable;
}

uint8_t
wlan_mlme_get_mgmt_hw_tx_retry_count(struct wlan_objmgr_psoc *psoc,
				     enum mlme_cfg_frame_type frm_type)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj)
		return 0;

	if (frm_type >= CFG_FRAME_TYPE_MAX)
		return 0;

	return mlme_obj->cfg.gen.mgmt_hw_tx_retry_count[frm_type];
}

QDF_STATUS
wlan_mlme_get_tx_retry_multiplier(struct wlan_objmgr_psoc *psoc,
				  uint32_t *tx_retry_multiplier)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);

	if (!mlme_obj) {
		*tx_retry_multiplier =
				cfg_default(CFG_TX_RETRY_MULTIPLIER);
		return QDF_STATUS_E_FAILURE;
	}

	*tx_retry_multiplier = mlme_obj->cfg.gen.tx_retry_multiplier;
	return QDF_STATUS_SUCCESS;
}
