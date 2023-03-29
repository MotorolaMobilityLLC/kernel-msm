/*
 * Copyright (c) 2013-2021 The Linux Foundation. All rights reserved.
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
 * DOC: target_if_cp_stats.c
 *
 * This file provide definition for APIs registered through lmac Tx Ops
 */

#include <qdf_mem.h>
#include <qdf_status.h>
#include <target_if_cp_stats.h>
#include <wmi_unified_priv.h>
#include <wmi_unified_param.h>
#include <target_if.h>
#include <wlan_tgt_def_config.h>
#include <wmi_unified_api.h>
#include <wlan_osif_priv.h>
#include <wlan_cp_stats_utils_api.h>
#include <wlan_cp_stats_mc_tgt_api.h>
#include "../../../umac/cmn_services/utils/inc/wlan_utility.h"
#include <cdp_txrx_cmn_struct.h>
#include <cdp_txrx_ops.h>
#include <cdp_txrx_stats_struct.h>
#include <cdp_txrx_host_stats.h>
#include <cdp_txrx_ctrl.h>
#include <cds_api.h>

#ifdef WLAN_SUPPORT_TWT

#include <wmi.h>
#include <wlan_cp_stats_mc_ucfg_api.h>

/**
 * target_if_twt_fill_peer_twt_session_params() - Fills peer twt session
 * parameter obtained from firmware into mc_cp_stats of peer
 * @mc_cp_stats: Pointer to Peer mc_cp stats
 * @twt_params: twt session parameters to be copied
 *
 * Return: None
 */
static void target_if_twt_fill_peer_twt_session_params
(
	struct peer_mc_cp_stats *mc_cp_stats,
	struct wmi_host_twt_session_stats_info *twt_params
)
{
	uint32_t event_type;
	int i = 0;

	if (!mc_cp_stats || !twt_params)
		return;

	if ((twt_params->event_type == HOST_TWT_SESSION_UPDATE) ||
	    (twt_params->event_type == HOST_TWT_SESSION_TEARDOWN)) {
		/* Update for a existing session, find by dialog_id */
		for (i = 0; i < TWT_PEER_MAX_SESSIONS; i++) {
			if (mc_cp_stats->twt_param[i].dialog_id !=
			    twt_params->dialog_id)
				continue;
			qdf_mem_copy(&mc_cp_stats->twt_param[i], twt_params,
				     sizeof(*twt_params));
			return;
		}
	} else if (twt_params->event_type == HOST_TWT_SESSION_SETUP) {
		/* New session, fill in any existing invalid session */
		for (i = 0; i < TWT_PEER_MAX_SESSIONS; i++) {
			event_type = mc_cp_stats->twt_param[i].event_type;
			if ((event_type != HOST_TWT_SESSION_SETUP) &&
			    (event_type != HOST_TWT_SESSION_UPDATE)) {
				qdf_mem_copy(&mc_cp_stats->twt_param[i],
					     twt_params,
					     sizeof(*twt_params));
				return;
			}
		}
	}

	target_if_err("Unable to save twt session params with dialog id %d",
		      twt_params->dialog_id);
}

/**
 * target_if_obtain_mc_cp_stat_obj() - Retrieves peer mc cp stats object
 * @peer_obj: peer object
 *
 * Return: mc cp stats object on success or NULL
 */
static struct peer_mc_cp_stats *
target_if_obtain_mc_cp_stat_obj(struct wlan_objmgr_peer *peer_obj)
{
	struct peer_cp_stats *cp_stats_peer_obj;
	struct peer_mc_cp_stats *mc_cp_stats;

	cp_stats_peer_obj = wlan_objmgr_peer_get_comp_private_obj
				(peer_obj, WLAN_UMAC_COMP_CP_STATS);
	if (!cp_stats_peer_obj) {
		target_if_err("cp peer stats obj err");
		return NULL;
	}

	mc_cp_stats = cp_stats_peer_obj->peer_stats;
	if (!mc_cp_stats) {
		target_if_err("mc stats obj err");
		return NULL;
	}
	return mc_cp_stats;
}

/**
 * target_if_twt_session_params_event_handler() - Handles twt session stats
 * event from firmware and store the per peer twt session parameters in
 * mc_cp_stats
 * @scn: scn handle
 * @evt_buf: data buffer for event
 * @evt_data_len: data length of event
 *
 * Return: 0 on success, else error values
 */
static int target_if_twt_session_params_event_handler(ol_scn_t scn,
						      uint8_t *evt_buf,
						      uint32_t evt_data_len)
{
	struct wlan_objmgr_psoc *psoc_obj;
	struct wlan_objmgr_peer *peer_obj;
	struct wmi_unified *wmi_hdl;
	struct wmi_host_twt_session_stats_info twt_params;
	struct wmi_twt_session_stats_event_param params = {0};
	struct peer_mc_cp_stats *mc_cp_stats;
	struct peer_cp_stats *peer_cp_stats_priv;
	uint32_t expected_len;
	int i;
	QDF_STATUS status;

	if (!scn || !evt_buf) {
		target_if_err("scn: 0x%pK, evt_buf: 0x%pK", scn, evt_buf);
		return -EINVAL;
	}

	psoc_obj = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc_obj) {
		target_if_err("psoc object is null!");
		return -EINVAL;
	}

	wmi_hdl = get_wmi_unified_hdl_from_psoc(psoc_obj);
	if (!wmi_hdl) {
		target_if_err("wmi_handle is null!");
		return -EINVAL;
	}

	status = wmi_extract_twt_session_stats_event(wmi_hdl, evt_buf, &params);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_err("Could not extract twt session stats event");
		return qdf_status_to_os_return(status);
	}

	if (params.num_sessions > TWT_PEER_MAX_SESSIONS) {
		target_if_err("no of twt sessions exceeded the max supported");
		return -EINVAL;
	}

	expected_len = (sizeof(wmi_pdev_twt_session_stats_event_fixed_param) +
			WMI_TLV_HDR_SIZE + (params.num_sessions *
			sizeof(wmi_twt_session_stats_info)));

	if (evt_data_len < expected_len) {
		target_if_err("Got invalid len of data from FW %d expected %d",
			      evt_data_len, expected_len);
		return -EINVAL;
	}

	for (i = 0; i < params.num_sessions; i++) {
		status = wmi_extract_twt_session_stats_data(wmi_hdl, evt_buf,
							    &params,
							    &twt_params, i);
		if (QDF_IS_STATUS_ERROR(status)) {
			target_if_err("Unable to extract twt params for idx %d",
				      i);
			return -EINVAL;
		}

		peer_obj = wlan_objmgr_get_peer_by_mac(psoc_obj,
						       twt_params.peer_mac,
						       WLAN_CP_STATS_ID);
		if (!peer_obj) {
			target_if_err("peer obj not found for "
				      QDF_MAC_ADDR_FMT,
				      QDF_MAC_ADDR_REF(twt_params.peer_mac));
			continue;
		}

		peer_cp_stats_priv = wlan_cp_stats_get_peer_stats_obj(peer_obj);
		if (!peer_cp_stats_priv) {
			target_if_err("peer_cp_stats_priv is null");
			continue;
		}

		mc_cp_stats = target_if_obtain_mc_cp_stat_obj(peer_obj);
		if (!mc_cp_stats) {
			target_if_err("Unable to retrieve mc cp stats obj for "
				      QDF_MAC_ADDR_FMT,
				      QDF_MAC_ADDR_REF(twt_params.peer_mac));
			wlan_objmgr_peer_release_ref(peer_obj,
						     WLAN_CP_STATS_ID);
			continue;
		}

		wlan_cp_stats_peer_obj_lock(peer_cp_stats_priv);
		target_if_twt_fill_peer_twt_session_params(mc_cp_stats,
							   &twt_params);
		wlan_cp_stats_peer_obj_unlock(peer_cp_stats_priv);

		wlan_objmgr_peer_release_ref(peer_obj, WLAN_CP_STATS_ID);
	}
	return 0;
}

/**
 * target_if_twt_session_params_unregister_evt_hdlr() - Unregister the
 * event handler registered for wmi event wmi_twt_session_stats_event_id
 * @psoc: psoc object
 *
 * Return: QDF_STATUS_SUCCESS on success or QDF error values on failure
 */
static QDF_STATUS
target_if_twt_session_params_unregister_evt_hdlr(struct wlan_objmgr_psoc *psoc)
{
	struct wmi_unified *wmi_handle;

	if (!psoc) {
		target_if_err("psoc obj in null!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("wmi_handle is null!");
		return QDF_STATUS_E_INVAL;
	}

	wmi_unified_unregister_event_handler(wmi_handle,
					     wmi_twt_session_stats_event_id);

	return QDF_STATUS_SUCCESS;
}

/**
 * target_if_twt_session_params_register_evt_hdlr() - Register a event
 * handler with wmi layer for wmi event wmi_twt_session_stats_event_id
 * @psoc: psoc object
 *
 * Return: QDF_STATUS_SUCCESS on success or QDF error values on failure
 */
static QDF_STATUS
target_if_twt_session_params_register_evt_hdlr(struct wlan_objmgr_psoc *psoc)
{
	int ret_val;
	struct wmi_unified *wmi_handle;

	if (!psoc) {
		target_if_err("psoc obj in null!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("wmi_handle is null!");
		return QDF_STATUS_E_INVAL;
	}

	ret_val = wmi_unified_register_event_handler(
			wmi_handle,
			wmi_twt_session_stats_event_id,
			target_if_twt_session_params_event_handler,
			WMI_RX_WORK_CTX);

	if (ret_val)
		target_if_err("Failed to register twt session stats event cb");

	return qdf_status_from_os_return(ret_val);
}
#else
static QDF_STATUS
target_if_twt_session_params_register_evt_hdlr(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
target_if_twt_session_params_unregister_evt_hdlr(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_SUPPORT_TWT */

#ifdef WLAN_FEATURE_MIB_STATS
static void target_if_cp_stats_free_mib_stats(struct stats_event *ev)
{
	qdf_mem_free(ev->mib_stats);
	ev->mib_stats = NULL;
}
#else
static void target_if_cp_stats_free_mib_stats(struct stats_event *ev)
{
}
#endif
static void target_if_cp_stats_free_stats_event(struct stats_event *ev)
{
	qdf_mem_free(ev->pdev_stats);
	ev->pdev_stats = NULL;
	qdf_mem_free(ev->peer_stats);
	ev->peer_stats = NULL;
	qdf_mem_free(ev->peer_adv_stats);
	ev->peer_adv_stats = NULL;
	qdf_mem_free(ev->peer_extended_stats);
	ev->peer_extended_stats = NULL;
	qdf_mem_free(ev->cca_stats);
	ev->cca_stats = NULL;
	qdf_mem_free(ev->vdev_summary_stats);
	ev->vdev_summary_stats = NULL;
	qdf_mem_free(ev->vdev_chain_rssi);
	ev->vdev_chain_rssi = NULL;
	target_if_cp_stats_free_mib_stats(ev);
	qdf_mem_free(ev->peer_stats_info_ext);
	ev->peer_stats_info_ext = NULL;
}

static QDF_STATUS target_if_cp_stats_extract_pdev_stats(
					struct wmi_unified *wmi_hdl,
					wmi_host_stats_event *stats_param,
					struct stats_event *ev,
					uint8_t *data)
{
	uint32_t i;
	QDF_STATUS status;
	wmi_host_pdev_stats pdev_stats;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	cdp_config_param_type val;

	ev->num_pdev_stats = stats_param->num_pdev_stats;
	if (!ev->num_pdev_stats)
		return QDF_STATUS_SUCCESS;

	/*
	 * num_pdev_stats is validated within function wmi_extract_stats_param
	 * which is called to populated wmi_host_stats_event stats_param
	 */
	ev->pdev_stats = qdf_mem_malloc(sizeof(*ev->pdev_stats) *
						ev->num_pdev_stats);
	if (!ev->pdev_stats)
		return QDF_STATUS_E_NOMEM;

	for (i = 0; i < ev->num_pdev_stats; i++) {
		status = wmi_extract_pdev_stats(wmi_hdl, data, i, &pdev_stats);
		if (QDF_IS_STATUS_ERROR(status)) {
			cp_stats_err("wmi_extract_pdev_stats failed");
			return status;
		}
		ev->pdev_stats[i].max_pwr = pdev_stats.chan_tx_pwr;

		val.cdp_pdev_param_chn_noise_flr = pdev_stats.chan_nf;
		cdp_txrx_set_pdev_param(soc, 0, CDP_CHAN_NOISE_FLOOR, val);
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS target_if_cp_stats_extract_pmf_bcn_protect_stats(
					struct wmi_unified *wmi_hdl,
					wmi_host_stats_event *stats_param,
					struct stats_event *ev, uint8_t *data)
{
	QDF_STATUS status;
	wmi_host_pmf_bcn_protect_stats pmf_bcn_stats = {0};

	if (!(stats_param->stats_id & WMI_HOST_REQUEST_PMF_BCN_PROTECT_STAT))
		return QDF_STATUS_SUCCESS;

	qdf_mem_zero(&ev->bcn_protect_stats, sizeof(ev->bcn_protect_stats));
	status = wmi_extract_pmf_bcn_protect_stats(wmi_hdl, data,
						   &pmf_bcn_stats);
	if (QDF_IS_STATUS_ERROR(status)) {
		cp_stats_err("wmi_extract_pmf_bcn_protect_stats failed");
		return status;
	}

	ev->bcn_protect_stats.pmf_bcn_stats_valid = true;
	ev->bcn_protect_stats.igtk_mic_fail_cnt =
			pmf_bcn_stats.igtk_mic_fail_cnt;
	ev->bcn_protect_stats.igtk_replay_cnt =
			pmf_bcn_stats.igtk_replay_cnt;
	ev->bcn_protect_stats.bcn_mic_fail_cnt =
			pmf_bcn_stats.bcn_mic_fail_cnt;
	ev->bcn_protect_stats.bcn_replay_cnt =
			pmf_bcn_stats.bcn_replay_cnt;

	return QDF_STATUS_SUCCESS;
}

static void target_if_cp_stats_extract_peer_extd_stats(
	struct wmi_unified *wmi_hdl,
	wmi_host_stats_event *stats_param,
	struct stats_event *ev,
	uint8_t *data)

{
	QDF_STATUS status;
	uint32_t i;
	wmi_host_peer_extd_stats peer_extd_stats;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct cdp_peer_stats *peer_stats;

	if (!stats_param->num_peer_extd_stats)
		return;

	ev->peer_extended_stats =
			qdf_mem_malloc(sizeof(*ev->peer_extended_stats) *
				       stats_param->num_peer_extd_stats);
	if (!ev->peer_extended_stats)
		return;

	ev->num_peer_extd_stats = stats_param->num_peer_extd_stats;

	for (i = 0; i < ev->num_peer_extd_stats; i++) {
		status = wmi_extract_peer_extd_stats(wmi_hdl, data, i,
						     &peer_extd_stats);
		if (QDF_IS_STATUS_ERROR(status)) {
			cp_stats_err("wmi_extract_peer_extd_stats failed");
			continue;
		}
		WMI_MAC_ADDR_TO_CHAR_ARRAY(
			     &peer_extd_stats.peer_macaddr,
			ev->peer_extended_stats[i].peer_macaddr);
		ev->peer_extended_stats[i].rx_mc_bc_cnt =
						peer_extd_stats.rx_mc_bc_cnt;

		peer_stats = qdf_mem_malloc(sizeof(*peer_stats));
		if (!peer_stats)
			continue;

		status = cdp_host_get_peer_stats(soc, VDEV_ALL,
					ev->peer_extended_stats[i].peer_macaddr,
					peer_stats);
		if (status == QDF_STATUS_SUCCESS)
			ev->peer_extended_stats[i].rx_mc_bc_cnt =
				peer_stats->rx.multicast.num +
				peer_stats->rx.bcast.num;

		qdf_mem_free(peer_stats);
	}
}

static QDF_STATUS target_if_cp_stats_extract_peer_stats(
					struct wmi_unified *wmi_hdl,
					wmi_host_stats_event *stats_param,
					struct stats_event *ev,
					uint8_t *data)
{
	uint32_t i;
	QDF_STATUS status;
	wmi_host_peer_stats peer_stats;
	bool db2dbm_enabled;
	struct wmi_host_peer_adv_stats *peer_adv_stats;

	/* Extract peer_stats */
	if (!stats_param->num_peer_stats)
		goto adv_stats;

	ev->peer_stats = qdf_mem_malloc(sizeof(*ev->peer_stats) *
						stats_param->num_peer_stats);
	if (!ev->peer_stats)
		return QDF_STATUS_E_NOMEM;
	ev->num_peer_stats = stats_param->num_peer_stats;

	db2dbm_enabled = wmi_service_enabled(wmi_hdl,
					     wmi_service_hw_db2dbm_support);
	for (i = 0; i < ev->num_peer_stats; i++) {
		status = wmi_extract_peer_stats(wmi_hdl, data, i, &peer_stats);
		if (QDF_IS_STATUS_ERROR(status)) {
			cp_stats_err("wmi_extract_peer_stats failed");
			continue;
		}
		WMI_MAC_ADDR_TO_CHAR_ARRAY(&peer_stats.peer_macaddr,
					   ev->peer_stats[i].peer_macaddr);
		ev->peer_stats[i].tx_rate = peer_stats.peer_tx_rate;
		ev->peer_stats[i].rx_rate = peer_stats.peer_rx_rate;
		if (db2dbm_enabled)
			ev->peer_stats[i].peer_rssi = peer_stats.peer_rssi;
		else
			ev->peer_stats[i].peer_rssi = peer_stats.peer_rssi +
							TGT_NOISE_FLOOR_DBM;
	}

adv_stats:
	target_if_cp_stats_extract_peer_extd_stats(wmi_hdl, stats_param, ev,
						   data);

	/* Extract peer_adv_stats */
	ev->num_peer_adv_stats = stats_param->num_peer_adv_stats;
	if (!ev->num_peer_adv_stats)
		return QDF_STATUS_SUCCESS;

	ev->peer_adv_stats = qdf_mem_malloc(sizeof(*ev->peer_adv_stats) *
					    ev->num_peer_adv_stats);
	if (!ev->peer_adv_stats)
		return QDF_STATUS_E_NOMEM;

	peer_adv_stats = qdf_mem_malloc(sizeof(*peer_adv_stats) *
					ev->num_peer_adv_stats);
	if (!peer_adv_stats) {
		qdf_mem_free(ev->peer_adv_stats);
		return QDF_STATUS_E_NOMEM;
	}

	status = wmi_extract_peer_adv_stats(wmi_hdl, data, peer_adv_stats);
	if (QDF_IS_STATUS_ERROR(status)) {
		cp_stats_err("wmi_extract_peer_stats failed");
		qdf_mem_free(peer_adv_stats);
		qdf_mem_free(ev->peer_adv_stats);
		ev->peer_adv_stats = NULL;
		return QDF_STATUS_SUCCESS;
	}

	for (i = 0; i < ev->num_peer_adv_stats; i++) {
		qdf_mem_copy(&ev->peer_adv_stats[i].peer_macaddr,
			     &peer_adv_stats[i].peer_macaddr,
			     QDF_MAC_ADDR_SIZE);
		ev->peer_adv_stats[i].fcs_count = peer_adv_stats[i].fcs_count;
		ev->peer_adv_stats[i].rx_bytes = peer_adv_stats[i].rx_bytes;
		ev->peer_adv_stats[i].rx_count = peer_adv_stats[i].rx_count;
	}
	qdf_mem_free(peer_adv_stats);
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS target_if_cp_stats_extract_cca_stats(
					struct wmi_unified *wmi_hdl,
					wmi_host_stats_event *stats_param,
					struct stats_event *ev, uint8_t *data)
{
	QDF_STATUS status;
	struct wmi_host_congestion_stats stats = {0};

	status = wmi_extract_cca_stats(wmi_hdl, data, &stats);
	if (QDF_IS_STATUS_ERROR(status))
		return QDF_STATUS_SUCCESS;

	ev->cca_stats = qdf_mem_malloc(sizeof(*ev->cca_stats));
	if (!ev->cca_stats)
		return QDF_STATUS_E_NOMEM;

	ev->cca_stats->vdev_id = stats.vdev_id;
	ev->cca_stats->congestion = stats.congestion;

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_MIB_STATS
static QDF_STATUS target_if_cp_stats_extract_mib_stats(
					struct wmi_unified *wmi_hdl,
					wmi_host_stats_event *stats_param,
					struct stats_event *ev, uint8_t *data)
{
	QDF_STATUS status;

	if (!stats_param->num_mib_stats)
		return QDF_STATUS_SUCCESS;

	if (stats_param->num_mib_stats != MAX_MIB_STATS ||
	    (stats_param->num_mib_extd_stats &&
	    stats_param->num_mib_extd_stats != MAX_MIB_STATS)) {
		cp_stats_err("number of mib stats wrong, num_mib_stats %d, num_mib_extd_stats %d",
			     stats_param->num_mib_stats,
			     stats_param->num_mib_extd_stats);
		return QDF_STATUS_E_INVAL;
	}

	ev->num_mib_stats = stats_param->num_mib_stats;

	ev->mib_stats = qdf_mem_malloc(sizeof(*ev->mib_stats));
	if (!ev->mib_stats)
		return QDF_STATUS_E_NOMEM;

	status = wmi_extract_mib_stats(wmi_hdl, data, ev->mib_stats);
	if (QDF_IS_STATUS_ERROR(status)) {
		cp_stats_err("wmi_extract_mib_stats failed");
		return status;
	}

	return QDF_STATUS_SUCCESS;
}
#else
static QDF_STATUS target_if_cp_stats_extract_mib_stats(
					struct wmi_unified *wmi_hdl,
					wmi_host_stats_event *stats_param,
					struct stats_event *ev, uint8_t *data)
{
	return QDF_STATUS_SUCCESS;
}
#endif

static QDF_STATUS target_if_cp_stats_extract_vdev_summary_stats(
					struct wmi_unified *wmi_hdl,
					wmi_host_stats_event *stats_param,
					struct stats_event *ev, uint8_t *data)
{
	uint32_t i, j;
	QDF_STATUS status;
	int32_t bcn_snr, dat_snr;
	wmi_host_vdev_stats vdev_stats;
	bool db2dbm_enabled;

	ev->num_summary_stats = stats_param->num_vdev_stats;
	if (!ev->num_summary_stats)
		return QDF_STATUS_SUCCESS;

	ev->vdev_summary_stats = qdf_mem_malloc(sizeof(*ev->vdev_summary_stats)
					* ev->num_summary_stats);

	if (!ev->vdev_summary_stats)
		return QDF_STATUS_E_NOMEM;

	db2dbm_enabled = wmi_service_enabled(wmi_hdl,
					     wmi_service_hw_db2dbm_support);
	for (i = 0; i < ev->num_summary_stats; i++) {
		status = wmi_extract_vdev_stats(wmi_hdl, data, i, &vdev_stats);
		if (QDF_IS_STATUS_ERROR(status))
			continue;

		bcn_snr = vdev_stats.vdev_snr.bcn_snr;
		dat_snr = vdev_stats.vdev_snr.dat_snr;
		ev->vdev_summary_stats[i].vdev_id = vdev_stats.vdev_id;

		cp_stats_debug("vdev %d SNR bcn: %d data: %d",
			       ev->vdev_summary_stats[i].vdev_id, bcn_snr,
			       dat_snr);

		for (j = 0; j < 4; j++) {
			ev->vdev_summary_stats[i].stats.tx_frm_cnt[j]
					= vdev_stats.tx_frm_cnt[j];
			ev->vdev_summary_stats[i].stats.fail_cnt[j]
					= vdev_stats.fail_cnt[j];
			ev->vdev_summary_stats[i].stats.multiple_retry_cnt[j]
					= vdev_stats.multiple_retry_cnt[j];
		}

		ev->vdev_summary_stats[i].stats.rx_frm_cnt =
						vdev_stats.rx_frm_cnt;
		ev->vdev_summary_stats[i].stats.rx_error_cnt =
						vdev_stats.rx_err_cnt;
		ev->vdev_summary_stats[i].stats.rx_discard_cnt =
						vdev_stats.rx_discard_cnt;
		ev->vdev_summary_stats[i].stats.ack_fail_cnt =
						vdev_stats.ack_fail_cnt;
		ev->vdev_summary_stats[i].stats.rts_succ_cnt =
						vdev_stats.rts_succ_cnt;
		ev->vdev_summary_stats[i].stats.rts_fail_cnt =
						vdev_stats.rts_fail_cnt;
		/* Update SNR and RSSI in SummaryStats */
		wlan_util_stats_get_rssi(db2dbm_enabled, bcn_snr, dat_snr,
					 &ev->vdev_summary_stats[i].stats.rssi);
		ev->vdev_summary_stats[i].stats.snr =
				ev->vdev_summary_stats[i].stats.rssi -
				TGT_NOISE_FLOOR_DBM;
	}

	return QDF_STATUS_SUCCESS;
}


static QDF_STATUS target_if_cp_stats_extract_vdev_chain_rssi_stats(
					struct wmi_unified *wmi_hdl,
					wmi_host_stats_event *stats_param,
					struct stats_event *ev, uint8_t *data)
{
	uint32_t i, j;
	QDF_STATUS status;
	int32_t bcn_snr, dat_snr;
	struct wmi_host_per_chain_rssi_stats rssi_stats;
	bool db2dbm_enabled;

	ev->num_chain_rssi_stats = stats_param->num_rssi_stats;
	if (!ev->num_chain_rssi_stats)
		return QDF_STATUS_SUCCESS;

	ev->vdev_chain_rssi = qdf_mem_malloc(sizeof(*ev->vdev_chain_rssi) *
						ev->num_chain_rssi_stats);
	if (!ev->vdev_chain_rssi)
		return QDF_STATUS_E_NOMEM;

	db2dbm_enabled = wmi_service_enabled(wmi_hdl,
					     wmi_service_hw_db2dbm_support);
	for (i = 0; i < ev->num_chain_rssi_stats; i++) {
		status = wmi_extract_per_chain_rssi_stats(wmi_hdl, data, i,
							  &rssi_stats);
		if (QDF_IS_STATUS_ERROR(status))
			continue;
		ev->vdev_chain_rssi[i].vdev_id = rssi_stats.vdev_id;

		for (j = 0; j < MAX_NUM_CHAINS; j++) {
			dat_snr = rssi_stats.rssi_avg_data[j];
			bcn_snr = rssi_stats.rssi_avg_beacon[j];
			cp_stats_nofl_debug("Chain %d SNR bcn: %d data: %d", j,
					    bcn_snr, dat_snr);
			/*
			 * Get the absolute rssi value from the current rssi
			 * value the snr value is hardcoded into 0 in the
			 * qcacld-new/CORE stack
			 */
			wlan_util_stats_get_rssi(db2dbm_enabled, bcn_snr,
						 dat_snr,
						 &ev->vdev_chain_rssi[i].
						 chain_rssi[j]);
		}
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS target_if_cp_stats_extract_event(struct wmi_unified *wmi_hdl,
						   struct stats_event *ev,
						   uint8_t *data)
{
	QDF_STATUS status;
	wmi_host_stats_event stats_param = {0};

	status = wmi_extract_stats_param(wmi_hdl, data, &stats_param);
	if (QDF_IS_STATUS_ERROR(status)) {
		cp_stats_err("stats param extract failed: %d", status);
		return status;
	}
	cp_stats_nofl_debug("num: pdev: %d, pdev_extd: %d, vdev: %d, peer: %d,"
			    "peer_extd: %d rssi: %d, mib %d, mib_extd %d, "
			    "bcnflt: %d, channel: %d, bcn: %d, peer_extd2: %d,"
			    "last_event: %x, stats id: %d",
			    stats_param.num_pdev_stats,
			    stats_param.num_pdev_ext_stats,
			    stats_param.num_vdev_stats,
			    stats_param.num_peer_stats,
			    stats_param.num_peer_extd_stats,
			    stats_param.num_rssi_stats,
			    stats_param.num_mib_stats,
			    stats_param.num_mib_extd_stats,
			    stats_param.num_bcnflt_stats,
			    stats_param.num_chan_stats,
			    stats_param.num_bcn_stats,
			    stats_param.num_peer_adv_stats,
			    stats_param.last_event,
			    stats_param.stats_id);

	ev->last_event = stats_param.last_event;
	status = target_if_cp_stats_extract_pdev_stats(wmi_hdl, &stats_param,
						       ev, data);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	status = target_if_cp_stats_extract_peer_stats(wmi_hdl, &stats_param,
						       ev, data);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	status = target_if_cp_stats_extract_cca_stats(wmi_hdl, &stats_param,
						      ev, data);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	status = target_if_cp_stats_extract_vdev_summary_stats(wmi_hdl,
							       &stats_param,
							       ev, data);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	status = target_if_cp_stats_extract_vdev_chain_rssi_stats(wmi_hdl,
								  &stats_param,
								  ev, data);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	status = target_if_cp_stats_extract_mib_stats(wmi_hdl,
						      &stats_param,
						      ev, data);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	status = target_if_cp_stats_extract_pmf_bcn_protect_stats(wmi_hdl,
								  &stats_param,
								  ev, data);

	return status;
}

uint8_t target_if_mc_cp_get_mac_id(struct vdev_mlme_obj *vdev_mlme)
{
	uint8_t mac_id = 0;

	if (wlan_reg_is_24ghz_ch_freq(vdev_mlme->vdev->vdev_mlme.des_chan->ch_freq))
		mac_id = TGT_MAC_ID_24G;
	else
		mac_id = TGT_MAC_ID_5G;

	return mac_id;
}

/**
 * target_if_mc_cp_stats_stats_event_handler() - function to handle stats event
 * from firmware.
 * @scn: scn handle
 * @data: data buffer for event
 * @datalen: data length
 *
 * Return: status of operation.
 */
static int target_if_mc_cp_stats_stats_event_handler(ol_scn_t scn,
						     uint8_t *data,
						     uint32_t datalen)
{
	QDF_STATUS status;
	struct stats_event ev = {0};
	struct wlan_objmgr_psoc *psoc;
	struct wmi_unified *wmi_handle;
	struct wlan_lmac_if_cp_stats_rx_ops *rx_ops;

	if (!scn || !data) {
		cp_stats_err("scn: 0x%pK, data: 0x%pK", scn, data);
		return -EINVAL;
	}
	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		cp_stats_err("null psoc");
		return -EINVAL;
	}

	rx_ops = target_if_cp_stats_get_rx_ops(psoc);
	if (!rx_ops || !rx_ops->process_stats_event) {
		cp_stats_err("callback not registered");
		return -EINVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		cp_stats_err("wmi_handle is null");
		return -EINVAL;
	}

	status = target_if_cp_stats_extract_event(wmi_handle, &ev, data);
	if (QDF_IS_STATUS_ERROR(status)) {
		cp_stats_err("extract event failed");
		goto end;
	}

	status = rx_ops->process_stats_event(psoc, &ev);

end:
	target_if_cp_stats_free_stats_event(&ev);

	return qdf_status_to_os_return(status);
}

#ifdef WLAN_FEATURE_BIG_DATA_STATS
static int target_if_mc_cp_stats_big_data_stats_event_handler(ol_scn_t scn,
							      uint8_t *data,
							      uint32_t datalen)
{
	QDF_STATUS status;
	struct big_data_stats_event ev = {0};
	struct wlan_objmgr_psoc *psoc;
	struct wmi_unified *wmi_handle;
	struct wlan_lmac_if_cp_stats_rx_ops *rx_ops;

	if (!scn || !data) {
		cp_stats_err("scn: 0x%pK, data: 0x%pK", scn, data);
		return -EINVAL;
	}
	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		cp_stats_err("null psoc");
		return -EINVAL;
	}

	rx_ops = target_if_cp_stats_get_rx_ops(psoc);
	if (!rx_ops || !rx_ops->process_big_data_stats_event) {
		cp_stats_err("callback not registered");
		return -EINVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		cp_stats_err("wmi_handle is null");
		return -EINVAL;
	}

	status = wmi_extract_big_data_stats_param(wmi_handle, data, &ev);
	if (QDF_IS_STATUS_ERROR(status)) {
		cp_stats_err("extract event failed");
		goto end;
	}

	status = rx_ops->process_big_data_stats_event(psoc, &ev);

end:
	return qdf_status_to_os_return(status);
}

/**
 * target_if_cp_stats_send_big_data_stats_req() - API to send request to wmi
 * @psoc: pointer to psoc object
 * @req: pointer to object containing stats request parameters
 *
 * Return: status of operation.
 */
static QDF_STATUS target_if_cp_stats_send_big_data_stats_req(
				struct wlan_objmgr_psoc *psoc,
				struct request_info *req)

{
	struct wmi_unified *wmi_handle;
	struct stats_request_params param = {0};

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		cp_stats_err("wmi_handle is null.");
		return QDF_STATUS_E_NULL_VALUE;
	}
	param.vdev_id = req->vdev_id;

	return wmi_unified_big_data_stats_request_send(wmi_handle,
						       &param);
}
#endif

static QDF_STATUS
target_if_cp_stats_extract_peer_stats_event(struct wmi_unified *wmi_hdl,
					    struct stats_event *ev,
					    uint8_t *data)
{
	QDF_STATUS status;
	wmi_host_stats_event stats_param = {0};
	struct peer_stats_info_ext_event *peer_stats_info;
	wmi_host_peer_stats_info stats_info;
	uint32_t peer_stats_info_size;
	int i, j;

	status = wmi_extract_peer_stats_param(wmi_hdl, data, &stats_param);
	if (QDF_IS_STATUS_ERROR(status)) {
		cp_stats_err("peer stats param extract failed: %d", status);
		return status;
	}

	if (stats_param.num_peer_stats_info_ext == 0) {
		cp_stats_err("num_peer_stats_info_ext is 0");
		return status;
	}

	ev->num_peer_stats_info_ext = stats_param.num_peer_stats_info_ext;
	peer_stats_info_size = sizeof(*ev->peer_stats_info_ext) *
			       ev->num_peer_stats_info_ext;
	ev->peer_stats_info_ext = qdf_mem_malloc(peer_stats_info_size);
	if (!ev->peer_stats_info_ext) {
		ev->num_peer_stats_info_ext = 0;
		return QDF_STATUS_E_NOMEM;
	}

	peer_stats_info = ev->peer_stats_info_ext;
	for (i = 0; i < ev->num_peer_stats_info_ext; i++) {
		status = wmi_extract_peer_stats_info(wmi_hdl, data,
						     i, &stats_info);
		if (QDF_IS_STATUS_ERROR(status)) {
			cp_stats_err("peer stats info extract failed: %d",
				     status);
			qdf_mem_free(ev->peer_stats_info_ext);
			ev->peer_stats_info_ext = NULL;
			ev->num_peer_stats_info_ext = 0;
			return status;
		}
		qdf_mem_copy(&peer_stats_info->peer_macaddr,
			     &stats_info.peer_macaddr,
			     sizeof(peer_stats_info->peer_macaddr));
		peer_stats_info->tx_packets = stats_info.tx_packets;
		peer_stats_info->tx_bytes = stats_info.tx_bytes;
		peer_stats_info->rx_packets = stats_info.rx_packets;
		peer_stats_info->rx_bytes = stats_info.rx_bytes;
		peer_stats_info->tx_retries = stats_info.tx_retries;
		peer_stats_info->tx_failed = stats_info.tx_failed;
		peer_stats_info->tx_succeed = stats_info.tx_succeed;
		peer_stats_info->rssi = stats_info.peer_rssi;
		peer_stats_info->tx_rate = stats_info.last_tx_bitrate_kbps;
		peer_stats_info->tx_rate_code = stats_info.last_tx_rate_code;
		peer_stats_info->rx_rate = stats_info.last_rx_bitrate_kbps;
		peer_stats_info->rx_rate_code = stats_info.last_rx_rate_code;
		for (j = 0; j < WMI_MAX_CHAINS; j++)
			peer_stats_info->peer_rssi_per_chain[j] =
					      stats_info.peer_rssi_per_chain[j];
		peer_stats_info++;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * target_if_mc_cp_stats_peer_stats_info_event_handler() - function to handle
 * peer stats info event from firmware.
 * @scn: scn handle
 * @data: data buffer for event
 * @datalen: data length
 *
 * Return: status of operation.
 */
static int target_if_mc_cp_stats_peer_stats_info_event_handler(ol_scn_t scn,
							       uint8_t *data,
							       uint32_t datalen)
{
	QDF_STATUS status;
	struct stats_event ev = {0};
	struct wlan_objmgr_psoc *psoc;
	struct wmi_unified *wmi_handle;
	struct wlan_lmac_if_cp_stats_rx_ops *rx_ops;

	if (!scn || !data) {
		cp_stats_err("scn: 0x%pK, data: 0x%pK", scn, data);
		return -EINVAL;
	}
	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		cp_stats_err("null psoc");
		return -EINVAL;
	}

	rx_ops = target_if_cp_stats_get_rx_ops(psoc);
	if (!rx_ops || !rx_ops->process_stats_event) {
		cp_stats_err("callback not registered");
		return -EINVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		cp_stats_err("wmi_handle is null");
		return -EINVAL;
	}

	status = target_if_cp_stats_extract_peer_stats_event(wmi_handle,
							     &ev, data);
	if (QDF_IS_STATUS_ERROR(status)) {
		cp_stats_err("extract event failed");
		goto end;
	}

	status = rx_ops->process_stats_event(psoc, &ev);

end:
	target_if_cp_stats_free_stats_event(&ev);

	return qdf_status_to_os_return(status);
}

static void target_if_cp_stats_inc_wake_lock_stats(uint32_t reason,
					struct wake_lock_stats *stats,
					uint32_t *unspecified_wake_count)
{
	switch (reason) {
	case WOW_REASON_UNSPECIFIED:
		(*unspecified_wake_count)++;
		break;

	case WOW_REASON_ASSOC_REQ_RECV:
		stats->mgmt_assoc++;
		break;

	case WOW_REASON_DISASSOC_RECVD:
		stats->mgmt_disassoc++;
		break;

	case WOW_REASON_ASSOC_RES_RECV:
		stats->mgmt_assoc_resp++;
		break;

	case WOW_REASON_REASSOC_REQ_RECV:
		stats->mgmt_reassoc++;
		break;

	case WOW_REASON_REASSOC_RES_RECV:
		stats->mgmt_reassoc_resp++;
		break;

	case WOW_REASON_AUTH_REQ_RECV:
		stats->mgmt_auth++;
		break;

	case WOW_REASON_DEAUTH_RECVD:
		stats->mgmt_deauth++;
		break;

	case WOW_REASON_ACTION_FRAME_RECV:
		stats->mgmt_action++;
		break;

	case WOW_REASON_NLOD:
		stats->pno_match_wake_up_count++;
		break;

	case WOW_REASON_NLO_SCAN_COMPLETE:
		stats->pno_complete_wake_up_count++;
		break;

	case WOW_REASON_LOW_RSSI:
		stats->low_rssi_wake_up_count++;
		break;

	case WOW_REASON_EXTSCAN:
		stats->gscan_wake_up_count++;
		break;

	case WOW_REASON_RSSI_BREACH_EVENT:
		stats->rssi_breach_wake_up_count++;
		break;

	case WOW_REASON_OEM_RESPONSE_EVENT:
		stats->oem_response_wake_up_count++;
		break;

	case WOW_REASON_11D_SCAN:
		stats->scan_11d++;
		break;

	case WOW_REASON_CHIP_POWER_FAILURE_DETECT:
		stats->pwr_save_fail_detected++;
		break;

	case WOW_REASON_LOCAL_DATA_UC_DROP:
		stats->uc_drop_wake_up_count++;
		break;

	case WOW_REASON_FATAL_EVENT_WAKE:
		stats->fatal_event_wake_up_count++;
		break;

	default:
		break;
	}
}

#ifdef WLAN_FEATURE_BIG_DATA_STATS
static QDF_STATUS
target_if_register_big_data_event_handler(struct wmi_unified *wmi_handle)
{
	return wmi_unified_register_event_handler(
			    wmi_handle, wmi_vdev_send_big_data_p2_eventid,
			    target_if_mc_cp_stats_big_data_stats_event_handler,
			    WMI_RX_WORK_CTX);
}

static void
target_if_unregister_big_data_event_handler(struct wmi_unified *wmi_handle)
{
	wmi_unified_unregister_event_handler(wmi_handle,
					     wmi_vdev_send_big_data_p2_eventid);
}

static QDF_STATUS
target_if_big_data_stats_register_tx_ops(struct wlan_lmac_if_cp_stats_tx_ops
					 *cp_stats_tx_ops)
{
	cp_stats_tx_ops->send_req_big_data_stats =
			target_if_cp_stats_send_big_data_stats_req;
	return QDF_STATUS_SUCCESS;
}

static void
target_if_big_data_stats_unregister_tx_ops(struct wlan_lmac_if_cp_stats_tx_ops
					   *cp_stats_tx_ops)
{
	cp_stats_tx_ops->send_req_big_data_stats = NULL;
}
#else
static QDF_STATUS
target_if_register_big_data_event_handler(struct wmi_unified *wmi_handle)
{
	return QDF_STATUS_SUCCESS;
}

static void
target_if_unregister_big_data_event_handler(struct wmi_unified *wmi_handle)
{}

static QDF_STATUS
target_if_big_data_stats_register_tx_ops(struct wlan_lmac_if_cp_stats_tx_ops
					   *cp_stats_tx_ops)
{
	return QDF_STATUS_SUCCESS;
}

static void
target_if_big_data_stats_unregister_tx_ops(struct wlan_lmac_if_cp_stats_tx_ops
					 *cp_stats_tx_ops)
{}
#endif

static QDF_STATUS
target_if_mc_cp_stats_register_event_handler(struct wlan_objmgr_psoc *psoc)
{
	int ret_val;
	struct wmi_unified *wmi_handle;

	if (!psoc) {
		cp_stats_err("PSOC is NULL!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		cp_stats_err("wmi_handle is null");
		return QDF_STATUS_E_INVAL;
	}

	ret_val = wmi_unified_register_event_handler(
			wmi_handle,
			wmi_update_stats_event_id,
			target_if_mc_cp_stats_stats_event_handler,
			WMI_RX_WORK_CTX);
	if (ret_val)
		cp_stats_err("Failed to register stats event cb");

	ret_val = wmi_unified_register_event_handler(wmi_handle,
			    wmi_peer_stats_info_event_id,
			    target_if_mc_cp_stats_peer_stats_info_event_handler,
			    WMI_RX_WORK_CTX);
	if (ret_val)
		cp_stats_err("Failed to register peer stats info event cb");

	ret_val = target_if_register_big_data_event_handler(wmi_handle);
	if (QDF_IS_STATUS_ERROR(ret_val))
		cp_stats_err("Failed to register big data stats info event cb");

	return qdf_status_from_os_return(ret_val);
}

static QDF_STATUS
target_if_mc_cp_stats_unregister_event_handler(struct wlan_objmgr_psoc *psoc)
{
	struct wmi_unified *wmi_handle;

	if (!psoc) {
		cp_stats_err("PSOC is NULL!");
		return QDF_STATUS_E_INVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		cp_stats_err("wmi_handle is null");
		return QDF_STATUS_E_INVAL;
	}

	target_if_unregister_big_data_event_handler(wmi_handle);

	wmi_unified_unregister_event_handler(wmi_handle,
					     wmi_peer_stats_info_event_id);
	wmi_unified_unregister_event_handler(wmi_handle,
					     wmi_update_stats_event_id);

	return QDF_STATUS_SUCCESS;
}

static uint32_t get_stats_id(enum stats_req_type type)
{
	switch (type) {
	default:
		break;
	case TYPE_CONNECTION_TX_POWER:
		return WMI_REQUEST_PDEV_STAT;
	case TYPE_PEER_STATS:
		return WMI_REQUEST_PEER_STAT | WMI_REQUEST_PEER_EXTD_STAT;
	case TYPE_STATION_STATS:
		return (WMI_REQUEST_AP_STAT   |
			WMI_REQUEST_PEER_STAT |
			WMI_REQUEST_VDEV_STAT |
			WMI_REQUEST_PDEV_STAT |
			WMI_REQUEST_PEER_EXTD2_STAT |
			WMI_REQUEST_RSSI_PER_CHAIN_STAT |
			WMI_REQUEST_PMF_BCN_PROTECT_STAT);
	case TYPE_MIB_STATS:
		return (WMI_REQUEST_MIB_STAT | WMI_REQUEST_MIB_EXTD_STAT);
	}

	return 0;
}

/**
 * target_if_cp_stats_send_stats_req() - API to send stats request to wmi
 * @psoc: pointer to psoc object
 * @req: pointer to object containing stats request parameters
 *
 * Return: status of operation.
 */
static QDF_STATUS target_if_cp_stats_send_stats_req(
					struct wlan_objmgr_psoc *psoc,
					enum stats_req_type type,
					struct request_info *req)

{
	struct wmi_unified *wmi_handle;
	struct stats_request_params param = {0};

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		cp_stats_err("wmi_handle is null.");
		return QDF_STATUS_E_NULL_VALUE;
	}

	/* refer  (WMI_REQUEST_STATS_CMDID) */
	param.stats_id = get_stats_id(type);
	param.vdev_id = req->vdev_id;
	param.pdev_id = req->pdev_id;

	/* only very frequent periodic stats needs to go over QMI.
	 * for that, wlan_hdd_qmi_get_sync_resume/wlan_hdd_qmi_put_suspend
	 * needs to be called to cover the period between qmi send and
	 * qmi resonse.
	 */
	if (TYPE_STATION_STATS == type)
		param.is_qmi_send_support = true;

	return wmi_unified_stats_request_send(wmi_handle, req->peer_mac_addr,
					      &param);
}

/**
 * target_if_mc_cp_stats_unregister_handlers() - Unregisters wmi event handlers
 * of control plane stats & twt session stats info
 * @psoc: PSOC object
 *
 * Return: QDF_STATUS_SUCCESS on success or QDF error values on failure
 */
static QDF_STATUS
target_if_mc_cp_stats_unregister_handlers(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS qdf_status;

	qdf_status = target_if_twt_session_params_unregister_evt_hdlr(psoc);
	if (qdf_status == QDF_STATUS_SUCCESS)
		qdf_status =
			target_if_mc_cp_stats_unregister_event_handler(psoc);

	return qdf_status;
}

/**
 * target_if_mc_cp_stats_register_handlers() - Registers wmi event handlers for
 * control plane stats & twt session stats info
 * @psoc: PSOC object
 *
 * Return: QDF_STATUS_SUCCESS on success or QDF error values on failure
 */
static QDF_STATUS
target_if_mc_cp_stats_register_handlers(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS qdf_status;

	qdf_status = target_if_mc_cp_stats_register_event_handler(psoc);
	if (qdf_status != QDF_STATUS_SUCCESS)
		return qdf_status;

	qdf_status = target_if_twt_session_params_register_evt_hdlr(psoc);
	if (qdf_status != QDF_STATUS_SUCCESS)
		target_if_mc_cp_stats_unregister_event_handler(psoc);

	return qdf_status;
}

/**
 * target_if_cp_stats_send_peer_stats_req() - API to send peer stats request
 * to wmi
 * @psoc: pointer to psoc object
 * @req: pointer to object containing peer stats request parameters
 *
 * Return: status of operation.
 */
static QDF_STATUS
target_if_cp_stats_send_peer_stats_req(struct wlan_objmgr_psoc *psoc,
				       struct request_info *req)

{
	struct wmi_unified *wmi_handle;
	struct peer_stats_request_params param = {0};

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		cp_stats_err("wmi_handle is null.");
		return QDF_STATUS_E_NULL_VALUE;
	}
	param.request_type = WMI_REQUEST_ONE_PEER_STATS_INFO;
	param.vdev_id = req->vdev_id;
	qdf_mem_copy(param.peer_mac_addr, req->peer_mac_addr,
		     QDF_MAC_ADDR_SIZE);
	param.reset_after_request = 0;

	return wmi_unified_peer_stats_request_send(wmi_handle, &param);
}

static QDF_STATUS
target_if_mc_cp_stats_register_tx_ops(struct wlan_lmac_if_tx_ops *tx_ops)
{
	struct wlan_lmac_if_cp_stats_tx_ops *cp_stats_tx_ops;

	if (!tx_ops) {
		cp_stats_err("lmac tx ops is NULL!");
		return QDF_STATUS_E_INVAL;
	}

	cp_stats_tx_ops = &tx_ops->cp_stats_tx_ops;
	if (!cp_stats_tx_ops) {
		cp_stats_err("lmac tx ops is NULL!");
		return QDF_STATUS_E_FAILURE;
	}

	cp_stats_tx_ops->inc_wake_lock_stats =
		target_if_cp_stats_inc_wake_lock_stats;
	cp_stats_tx_ops->send_req_stats = target_if_cp_stats_send_stats_req;
	cp_stats_tx_ops->send_req_peer_stats =
		target_if_cp_stats_send_peer_stats_req;

	target_if_big_data_stats_register_tx_ops(cp_stats_tx_ops);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
target_if_mc_cp_stats_unregister_tx_ops(struct wlan_lmac_if_tx_ops *tx_ops)
{
	struct wlan_lmac_if_cp_stats_tx_ops *cp_stats_tx_ops;

	if (!tx_ops) {
		cp_stats_err("lmac tx ops is NULL!");
		return QDF_STATUS_E_INVAL;
	}

	cp_stats_tx_ops = &tx_ops->cp_stats_tx_ops;
	if (!cp_stats_tx_ops) {
		cp_stats_err("lmac tx ops is NULL!");
		return QDF_STATUS_E_FAILURE;
	}
	target_if_big_data_stats_unregister_tx_ops(cp_stats_tx_ops);
	cp_stats_tx_ops->inc_wake_lock_stats = NULL;
	cp_stats_tx_ops->send_req_stats = NULL;
	cp_stats_tx_ops->send_req_peer_stats = NULL;

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_SUPPORT_LEGACY_CP_STATS_HANDLERS
QDF_STATUS
target_if_cp_stats_register_legacy_event_handler(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;
	struct wlan_lmac_if_tx_ops *tx_ops;

	tx_ops = wlan_psoc_get_lmac_if_txops(psoc);
	if (!tx_ops) {
		cp_stats_err("lmac tx ops is NULL!");
		return QDF_STATUS_E_FAILURE;
	}

	status = target_if_mc_cp_stats_register_tx_ops(tx_ops);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	return target_if_mc_cp_stats_register_handlers(psoc);
}

QDF_STATUS
target_if_cp_stats_unregister_legacy_event_handler(
						struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;
	struct wlan_lmac_if_tx_ops *tx_ops;

	tx_ops = wlan_psoc_get_lmac_if_txops(psoc);
	if (!tx_ops) {
		cp_stats_err("lmac tx ops is NULL!");
		return QDF_STATUS_E_FAILURE;
	}

	status = target_if_mc_cp_stats_unregister_tx_ops(tx_ops);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	return target_if_mc_cp_stats_unregister_handlers(psoc);
}
#endif
