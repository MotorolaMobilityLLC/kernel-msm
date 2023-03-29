/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_cp_stats_mc_ucfg_api.c
 *
 * This file provide API definitions required for northbound interaction
 */

#include <wlan_objmgr_psoc_obj.h>
#include "wlan_cp_stats_mc_defs.h"
#include <wlan_cp_stats_mc_ucfg_api.h>
#include <wlan_cp_stats_mc_tgt_api.h>
#include <wlan_cp_stats_utils_api.h>
#include "../../core/src/wlan_cp_stats_defs.h"
#include "../../core/src/wlan_cp_stats_cmn_api_i.h"
#ifdef WLAN_POWER_MANAGEMENT_OFFLOAD
#include <wlan_pmo_obj_mgmt_api.h>
#endif
#ifdef WLAN_SUPPORT_TWT
#include <wlan_mlme_twt_public_struct.h>
#endif
#include <wlan_mlme_api.h>

#ifdef WLAN_SUPPORT_TWT

/**
 * ucfg_twt_get_peer_session_param_by_dlg_id() - Finds a Peer twt session with
 * dialog id matching with input dialog id. If a match is found copies
 * the twt session parameters
 * @mc_stats: pointer to peer specific stats
 * @input_dialog_id: input dialog id
 * @dest_param: Pointer to copy twt session parameters when a peer with
 * given dialog id is found
 * @num_twt_session: Pointer holding total number of valid twt session
 *
 * Return: Success if stats are copied for a peer with given dialog,
 * else failure
 */
static QDF_STATUS
ucfg_twt_get_peer_session_param_by_dlg_id(struct peer_mc_cp_stats *mc_stats,
					  uint32_t input_dialog_id,
					  struct wmi_host_twt_session_stats_info
					  *dest_param, int *num_twt_session)
{
	struct wmi_host_twt_session_stats_info *src_param;
	uint32_t event_type;
	int i = 0;
	QDF_STATUS qdf_status = QDF_STATUS_E_INVAL;

	if (!mc_stats || !dest_param)
		return qdf_status;

	for (i = 0; i < TWT_PEER_MAX_SESSIONS; i++) {
		event_type = mc_stats->twt_param[i].event_type;

		src_param = &mc_stats->twt_param[i];
		if (!event_type ||
		    (src_param->dialog_id != input_dialog_id &&
		     input_dialog_id != WLAN_ALL_SESSIONS_DIALOG_ID))
			continue;

		if ((event_type == HOST_TWT_SESSION_SETUP) ||
		    (event_type == HOST_TWT_SESSION_UPDATE)) {
			qdf_mem_copy(&dest_param[*num_twt_session], src_param,
				     sizeof(*src_param));
			qdf_status = QDF_STATUS_SUCCESS;
			*num_twt_session += 1;
			if (*num_twt_session >= TWT_PEER_MAX_SESSIONS)
				break;
		}
	}

	return qdf_status;
}

/**
 * ucfg_twt_get_single_peer_session_params()- Extracts twt session parameters
 * corresponding to a peer given by dialog_id
 * @psoc_obj: psoc object
 * @mac_addr: mac addr of peer
 * @dialog_id: dialog id of peer for which twt session params to be retrieved
 * @params: pointer to store peer twt session parameters
 *
 * Return: total number of valid twt session
 */
static int
ucfg_twt_get_single_peer_session_params(struct wlan_objmgr_psoc *psoc_obj,
					uint8_t *mac_addr, uint32_t dialog_id,
					struct wmi_host_twt_session_stats_info
					*params)
{
	struct wlan_objmgr_peer *peer;
	struct peer_cp_stats *peer_cp_stats_priv;
	struct peer_mc_cp_stats *peer_mc_stats;
	QDF_STATUS qdf_status = QDF_STATUS_E_INVAL;
	int num_twt_session = 0;

	if (!psoc_obj || !params)
		return num_twt_session;

	peer = wlan_objmgr_get_peer_by_mac(psoc_obj, mac_addr,
					   WLAN_CP_STATS_ID);
	if (!peer)
		return num_twt_session;

	peer_cp_stats_priv = wlan_cp_stats_get_peer_stats_obj(peer);
	if (!peer_cp_stats_priv) {
		wlan_objmgr_peer_release_ref(peer, WLAN_CP_STATS_ID);
		return num_twt_session;
	}

	wlan_cp_stats_peer_obj_lock(peer_cp_stats_priv);
	peer_mc_stats = peer_cp_stats_priv->peer_stats;

	qdf_status = ucfg_twt_get_peer_session_param_by_dlg_id(
							peer_mc_stats,
							dialog_id,
							params,
							&num_twt_session);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		qdf_err("No TWT session for " QDF_MAC_ADDR_FMT " dialog_id %d",
			QDF_MAC_ADDR_REF(mac_addr), dialog_id);
	}

	wlan_cp_stats_peer_obj_unlock(peer_cp_stats_priv);
	wlan_objmgr_peer_release_ref(peer, WLAN_CP_STATS_ID);

	return num_twt_session;
}

/**
 * ucfg_twt_get_peer_session_param() - Obtains twt session parameters of
 * a peer if twt session is valid
 * @mc_cp_stats: pointer to peer specific stats
 * @param: Pointer to copy twt session parameters
 * @num_twt_sessions Pointer holding total number of valid twt sessions
 *
 * Return: QDF_STATUS success if valid twt session parameters are obtained
 * else other qdf error values
 */
static QDF_STATUS
ucfg_twt_get_peer_session_param(struct peer_mc_cp_stats *mc_cp_stats,
				struct wmi_host_twt_session_stats_info *params,
				int *num_twt_session)
{
	struct wmi_host_twt_session_stats_info *twt_params;
	QDF_STATUS qdf_status = QDF_STATUS_E_INVAL;
	uint32_t event_type;
	int i;

	if (!mc_cp_stats || !params)
		return qdf_status;

	for (i = 0; i < TWT_PEER_MAX_SESSIONS; i++) {
		twt_params = &mc_cp_stats->twt_param[i];
		event_type = mc_cp_stats->twt_param[i].event_type;

		/* Check twt session is established */
		if ((event_type == HOST_TWT_SESSION_SETUP) ||
		    (event_type == HOST_TWT_SESSION_UPDATE)) {
			qdf_mem_copy(&params[*num_twt_session], twt_params,
				     sizeof(*twt_params));
			qdf_status = QDF_STATUS_SUCCESS;
			*num_twt_session += 1;
		}
	}
	return qdf_status;
}

/**
 * ucfg_twt_get_all_peer_session_params()- Retrieves twt session parameters
 * of all peers with valid twt session
 * @psoc_obj: psoc object
 * @vdvev_id: vdev_id
 * @params: array of pointer to store peer twt session parameters
 *
 * Return: total number of valid twt sessions
 */
static int
ucfg_twt_get_all_peer_session_params(struct wlan_objmgr_psoc *psoc_obj,
				     uint8_t vdev_id,
				     struct wmi_host_twt_session_stats_info
				     *params)
{
	qdf_list_t *peer_list;
	struct wlan_objmgr_peer *peer, *peer_next;
	struct wlan_objmgr_vdev *vdev;
	struct peer_cp_stats *cp_stats_peer_obj, *peer_cp_stat_prv;
	struct peer_mc_cp_stats *mc_cp_stats;
	int num_twt_session = 0;
	enum QDF_OPMODE opmode;
	int sap_max_peer;

	if (!psoc_obj) {
		cp_stats_err("psoc is NULL");
		return num_twt_session;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc_obj, vdev_id,
						    WLAN_CP_STATS_ID);

	if (!vdev) {
		cp_stats_err("vdev is NULL, vdev_id: %d", vdev_id);
		return num_twt_session;
	}

	wlan_mlme_get_sap_max_peers(psoc_obj, &sap_max_peer);
	opmode = wlan_vdev_mlme_get_opmode(vdev);

	peer_list = &vdev->vdev_objmgr.wlan_peer_list;
	if (!peer_list) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_CP_STATS_ID);
		cp_stats_err("Peer list for vdev obj is NULL");
		return num_twt_session;
	}

	peer = wlan_vdev_peer_list_peek_active_head(vdev, peer_list,
						    WLAN_CP_STATS_ID);

	while (peer) {
		cp_stats_peer_obj = wlan_objmgr_peer_get_comp_private_obj(
						peer, WLAN_UMAC_COMP_CP_STATS);

		mc_cp_stats = NULL;
		if (cp_stats_peer_obj)
			mc_cp_stats = cp_stats_peer_obj->peer_stats;

		peer_cp_stat_prv =
			wlan_cp_stats_get_peer_stats_obj(peer);

		if (peer_cp_stat_prv && mc_cp_stats) {
			wlan_cp_stats_peer_obj_lock(peer_cp_stat_prv);
			ucfg_twt_get_peer_session_param(mc_cp_stats,
							params,
							&num_twt_session);
			wlan_cp_stats_peer_obj_unlock(peer_cp_stat_prv);
		}

		if (opmode == QDF_STA_MODE &&
		    num_twt_session >= TWT_PEER_MAX_SESSIONS) {
			wlan_objmgr_peer_release_ref(peer, WLAN_CP_STATS_ID);
			goto done;
		}

		if (opmode == QDF_SAP_MODE &&
		    num_twt_session >= (sap_max_peer * TWT_PEER_MAX_SESSIONS)) {
			wlan_objmgr_peer_release_ref(peer, WLAN_CP_STATS_ID);
			goto done;
		}

		peer_next = wlan_peer_get_next_active_peer_of_vdev(
							vdev, peer_list, peer,
							WLAN_CP_STATS_ID);
		wlan_objmgr_peer_release_ref(peer, WLAN_CP_STATS_ID);
		peer = peer_next;
	}

done:
	if (!num_twt_session)
		cp_stats_err("Unable to find a peer with twt session established");

	wlan_objmgr_vdev_release_ref(vdev, WLAN_CP_STATS_ID);
	return num_twt_session;
}

int
ucfg_twt_get_peer_session_params(struct wlan_objmgr_psoc *psoc_obj,
				 struct wmi_host_twt_session_stats_info *params)
{
	uint8_t *mac_addr;
	uint32_t dialog_id;
	uint8_t vdev_id;
	int num_twt_session = 0;

	if (!psoc_obj || !params)
		return num_twt_session;

	mac_addr = params[0].peer_mac;
	dialog_id = params[0].dialog_id;
	vdev_id = params[0].vdev_id;

	/*
	 * Currently for STA case, twt_get_params nl is sending only dialog_id
	 * and mac_addr is being filled by driver in STA peer case.
	 * For SAP case, twt_get_params nl is sending dialog_id and
	 * peer mac_addr. When twt_get_params add mac_addr and dialog_id of
	 * STA/SAP, we need handle unicast/multicast macaddr in
	 * ucfg_twt_get_peer_session_params.
	 */
	if (!QDF_IS_ADDR_BROADCAST(mac_addr))
		num_twt_session = ucfg_twt_get_single_peer_session_params(
								psoc_obj,
								mac_addr,
								dialog_id,
								params);
	else
		num_twt_session = ucfg_twt_get_all_peer_session_params(
								psoc_obj,
								vdev_id,
								params);

	return num_twt_session;
}
#endif /* WLAN_SUPPORT_TWT */

QDF_STATUS wlan_cp_stats_psoc_cs_init(struct psoc_cp_stats *psoc_cs)
{
	psoc_cs->obj_stats = qdf_mem_malloc(sizeof(struct psoc_mc_cp_stats));
	if (!psoc_cs->obj_stats)
		return QDF_STATUS_E_NOMEM;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_cp_stats_psoc_cs_deinit(struct psoc_cp_stats *psoc_cs)
{
	qdf_mem_free(psoc_cs->obj_stats);
	psoc_cs->obj_stats = NULL;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_cp_stats_vdev_cs_init(struct vdev_cp_stats *vdev_cs)
{
	vdev_cs->vdev_stats = qdf_mem_malloc(sizeof(struct vdev_mc_cp_stats));
	if (!vdev_cs->vdev_stats)
		return QDF_STATUS_E_NOMEM;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_cp_stats_vdev_cs_deinit(struct vdev_cp_stats *vdev_cs)
{
	qdf_mem_free(vdev_cs->vdev_stats);
	vdev_cs->vdev_stats = NULL;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_cp_stats_pdev_cs_init(struct pdev_cp_stats *pdev_cs)
{
	pdev_cs->pdev_stats = qdf_mem_malloc(sizeof(struct pdev_mc_cp_stats));
	if (!pdev_cs->pdev_stats)
		return QDF_STATUS_E_NOMEM;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_cp_stats_pdev_cs_deinit(struct pdev_cp_stats *pdev_cs)
{
	qdf_mem_free(pdev_cs->pdev_stats);
	pdev_cs->pdev_stats = NULL;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_cp_stats_peer_cs_init(struct peer_cp_stats *peer_cs)
{
	struct peer_mc_cp_stats *peer_mc_stats;

	peer_mc_stats = qdf_mem_malloc(sizeof(struct peer_mc_cp_stats));
	if (!peer_mc_stats)
		return QDF_STATUS_E_NOMEM;

	peer_mc_stats->adv_stats =
			qdf_mem_malloc(sizeof(struct peer_adv_mc_cp_stats));

	if (!peer_mc_stats->adv_stats) {
		qdf_mem_free(peer_mc_stats);
		peer_mc_stats = NULL;
		return QDF_STATUS_E_NOMEM;
	}

	peer_mc_stats->extd_stats =
			qdf_mem_malloc(sizeof(struct peer_extd_stats));

	if (!peer_mc_stats->extd_stats) {
		qdf_mem_free(peer_mc_stats->adv_stats);
		peer_mc_stats->adv_stats = NULL;
		qdf_mem_free(peer_mc_stats);
		peer_mc_stats = NULL;
		return QDF_STATUS_E_NOMEM;
	}
	peer_cs->peer_stats = peer_mc_stats;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_cp_stats_peer_cs_deinit(struct peer_cp_stats *peer_cs)
{
	struct peer_mc_cp_stats *peer_mc_stats = peer_cs->peer_stats;

	qdf_mem_free(peer_mc_stats->adv_stats);
	peer_mc_stats->adv_stats = NULL;
	qdf_mem_free(peer_mc_stats->extd_stats);
	peer_mc_stats->extd_stats = NULL;
	qdf_mem_free(peer_cs->peer_stats);
	peer_cs->peer_stats = NULL;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_mc_cp_stats_inc_wake_lock_stats_by_protocol(
					struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id,
					enum qdf_proto_subtype protocol)
{
	struct wake_lock_stats *stats;
	struct psoc_cp_stats *psoc_cp_stats_priv;
	struct psoc_mc_cp_stats *psoc_mc_stats;

	psoc_cp_stats_priv = wlan_cp_stats_get_psoc_stats_obj(psoc);
	if (!psoc_cp_stats_priv) {
		cp_stats_err("psoc cp stats object is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wlan_cp_stats_psoc_obj_lock(psoc_cp_stats_priv);
	psoc_mc_stats = psoc_cp_stats_priv->obj_stats;

	if (!psoc_mc_stats) {
		cp_stats_err("psoc mc stats is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	stats = &psoc_mc_stats->wow_stats;
	switch (protocol) {
	case QDF_PROTO_ICMP_REQ:
	case QDF_PROTO_ICMP_RES:
		stats->icmpv4_count++;
		break;
	case QDF_PROTO_ICMPV6_REQ:
	case QDF_PROTO_ICMPV6_RES:
	case QDF_PROTO_ICMPV6_RS:
		stats->icmpv6_count++;
		break;
	case QDF_PROTO_ICMPV6_RA:
		stats->icmpv6_count++;
		stats->ipv6_mcast_ra_stats++;
		break;
	case QDF_PROTO_ICMPV6_NS:
		stats->icmpv6_count++;
		stats->ipv6_mcast_ns_stats++;
		break;
	case QDF_PROTO_ICMPV6_NA:
		stats->icmpv6_count++;
		stats->ipv6_mcast_na_stats++;
		break;
	default:
		break;
	}
	wlan_cp_stats_psoc_obj_unlock(psoc_cp_stats_priv);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_mc_cp_stats_inc_wake_lock_stats_by_dst_addr(
					struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id, uint8_t *dest_mac)
{
	struct psoc_cp_stats *psoc_cp_stats_priv;
	struct psoc_mc_cp_stats *psoc_mc_stats;
	struct wake_lock_stats *stats;

	psoc_cp_stats_priv = wlan_cp_stats_get_psoc_stats_obj(psoc);
	if (!psoc_cp_stats_priv) {
		cp_stats_err("psoc cp stats object is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wlan_cp_stats_psoc_obj_lock(psoc_cp_stats_priv);
	psoc_mc_stats = psoc_cp_stats_priv->obj_stats;
	if (!psoc_mc_stats) {
		cp_stats_err("psoc mc stats is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	stats = &psoc_mc_stats->wow_stats;

	switch (*dest_mac) {
	case QDF_BCAST_MAC_ADDR:
		stats->bcast_wake_up_count++;
		break;
	case QDF_MCAST_IPV4_MAC_ADDR:
		stats->ipv4_mcast_wake_up_count++;
		break;
	case QDF_MCAST_IPV6_MAC_ADDR:
		stats->ipv6_mcast_wake_up_count++;
		break;
	default:
		stats->ucast_wake_up_count++;
		break;
	}
	wlan_cp_stats_psoc_obj_unlock(psoc_cp_stats_priv);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_mc_cp_stats_inc_wake_lock_stats(struct wlan_objmgr_psoc *psoc,
						uint8_t vdev_id,
						uint32_t reason)
{
	struct wake_lock_stats *stats;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct psoc_mc_cp_stats *psoc_mc_stats;
	struct psoc_cp_stats *psoc_cp_stats_priv;

	psoc_cp_stats_priv = wlan_cp_stats_get_psoc_stats_obj(psoc);
	if (!psoc_cp_stats_priv) {
		cp_stats_err("psoc cp stats object is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wlan_cp_stats_psoc_obj_lock(psoc_cp_stats_priv);

	psoc_mc_stats = psoc_cp_stats_priv->obj_stats;

	if (!psoc_mc_stats) {
		cp_stats_err("psoc mc stats is null");
		wlan_cp_stats_psoc_obj_unlock(psoc_cp_stats_priv);
		return QDF_STATUS_E_NULL_VALUE;
	}

	stats = &psoc_mc_stats->wow_stats;

	status = tgt_mc_cp_stats_inc_wake_lock_stats(psoc, reason, stats,
				&psoc_mc_stats->wow_unspecified_wake_up_count);
	wlan_cp_stats_psoc_obj_unlock(psoc_cp_stats_priv);

	return status;
}

/**
 * vdev_iterator() - iterator function to collect wake_lock_stats from all vdev
 * @psoc: pointer to psoc object
 * @vdev: pointer to vdev object
 * @arg: stats object pointer passed as arg
 *
 * Return - none
 */
static void vdev_iterator(struct wlan_objmgr_psoc *psoc, void *vdev, void *arg)
{
	struct wake_lock_stats *vdev_stats;
	struct wake_lock_stats *stats = arg;
	struct psoc_cp_stats *psoc_cp_stats_priv;
	struct psoc_mc_cp_stats *psoc_mc_stats;

	psoc_cp_stats_priv = wlan_cp_stats_get_psoc_stats_obj(psoc);
	if (!psoc_cp_stats_priv) {
		cp_stats_err("psoc cp stats object is null");
		return;
	}

	psoc_mc_stats = psoc_cp_stats_priv->obj_stats;
	if (!psoc_mc_stats) {
		cp_stats_err("psoc mc stats is null");
		return;
	}

	vdev_stats = &psoc_mc_stats->wow_stats;

	stats->ucast_wake_up_count += vdev_stats->ucast_wake_up_count;
	stats->bcast_wake_up_count += vdev_stats->bcast_wake_up_count;
	stats->ipv4_mcast_wake_up_count += vdev_stats->ipv4_mcast_wake_up_count;
	stats->ipv6_mcast_wake_up_count += vdev_stats->ipv6_mcast_wake_up_count;
	stats->ipv6_mcast_ra_stats += vdev_stats->ipv6_mcast_ra_stats;
	stats->ipv6_mcast_ns_stats += vdev_stats->ipv6_mcast_ns_stats;
	stats->ipv6_mcast_na_stats += vdev_stats->ipv6_mcast_na_stats;
	stats->icmpv4_count += vdev_stats->icmpv4_count;
	stats->icmpv6_count += vdev_stats->icmpv6_count;
	stats->rssi_breach_wake_up_count +=
			vdev_stats->rssi_breach_wake_up_count;
	stats->low_rssi_wake_up_count += vdev_stats->low_rssi_wake_up_count;
	stats->gscan_wake_up_count += vdev_stats->gscan_wake_up_count;
	stats->pno_complete_wake_up_count +=
			vdev_stats->pno_complete_wake_up_count;
	stats->pno_match_wake_up_count += vdev_stats->pno_match_wake_up_count;
	stats->oem_response_wake_up_count +=
			vdev_stats->oem_response_wake_up_count;
	stats->uc_drop_wake_up_count += vdev_stats->uc_drop_wake_up_count;
	stats->fatal_event_wake_up_count +=
			vdev_stats->fatal_event_wake_up_count;
	stats->pwr_save_fail_detected += vdev_stats->pwr_save_fail_detected;
	stats->scan_11d += vdev_stats->scan_11d;
}

QDF_STATUS ucfg_mc_cp_stats_get_psoc_wake_lock_stats(
						struct wlan_objmgr_psoc *psoc,
						struct wake_lock_stats *stats)
{
	struct psoc_cp_stats *psoc_cp_stats_priv;
	struct psoc_mc_cp_stats *psoc_mc_stats;

	psoc_cp_stats_priv = wlan_cp_stats_get_psoc_stats_obj(psoc);
	if (!psoc_cp_stats_priv) {
		cp_stats_err("psoc cp stats object is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wlan_cp_stats_psoc_obj_lock(psoc_cp_stats_priv);
	psoc_mc_stats = psoc_cp_stats_priv->obj_stats;
	/* iterate through all vdevs, and get wow stats from vdev_cs object */
	wlan_objmgr_iterate_obj_list(psoc, WLAN_VDEV_OP, vdev_iterator,
				     stats, true, WLAN_CP_STATS_ID);
	wlan_cp_stats_psoc_obj_unlock(psoc_cp_stats_priv);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_mc_cp_stats_get_vdev_wake_lock_stats(
						struct wlan_objmgr_vdev *vdev,
						struct wake_lock_stats *stats)
{
	struct wlan_objmgr_psoc *psoc;
	struct psoc_cp_stats *psoc_cp_stats_priv;
	struct psoc_mc_cp_stats *psoc_mc_stats;

	wlan_vdev_obj_lock(vdev);
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		wlan_vdev_obj_unlock(vdev);
		cp_stats_err("psoc NULL");
		return QDF_STATUS_E_INVAL;
	}
	wlan_vdev_obj_unlock(vdev);

	psoc_cp_stats_priv = wlan_cp_stats_get_psoc_stats_obj(psoc);
	if (!psoc_cp_stats_priv) {
		cp_stats_err("psoc cp stats object is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wlan_cp_stats_psoc_obj_lock(psoc_cp_stats_priv);
	psoc_mc_stats = psoc_cp_stats_priv->obj_stats;

	if (!psoc_mc_stats) {
		cp_stats_err("psoc mc stats is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	qdf_mem_copy(stats, &psoc_mc_stats->wow_stats, sizeof(*stats));

	wlan_cp_stats_psoc_obj_unlock(psoc_cp_stats_priv);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_mc_cp_stats_write_wow_stats(
				struct wlan_objmgr_psoc *psoc,
				char *buffer, uint16_t max_len, int *ret)
{
	QDF_STATUS status;
	uint32_t unspecified_wake_count;
	struct wake_lock_stats wow_stats = {0};
	struct psoc_mc_cp_stats *psoc_mc_stats;
	struct psoc_cp_stats *psoc_cp_stats_priv;

	psoc_cp_stats_priv = wlan_cp_stats_get_psoc_stats_obj(psoc);
	if (!psoc_cp_stats_priv) {
		cp_stats_err("psoc cp stats object is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	/* get stats from psoc */
	status = ucfg_mc_cp_stats_get_psoc_wake_lock_stats(psoc, &wow_stats);
	if (QDF_IS_STATUS_ERROR(status)) {
		cp_stats_err("Failed to get WoW stats");
		return status;
	}

	wlan_cp_stats_psoc_obj_lock(psoc_cp_stats_priv);
	psoc_mc_stats = psoc_cp_stats_priv->obj_stats;
	unspecified_wake_count = psoc_mc_stats->wow_unspecified_wake_up_count;
	wlan_cp_stats_psoc_obj_unlock(psoc_cp_stats_priv);

	*ret = qdf_scnprintf(buffer, max_len,
			     "WoW Wake Reasons\n"
			     "\tunspecified wake count: %u\n"
			     "\tunicast: %u\n"
			     "\tbroadcast: %u\n"
			     "\tIPv4 multicast: %u\n"
			     "\tIPv6 multicast: %u\n"
			     "\tIPv6 multicast RA: %u\n"
			     "\tIPv6 multicast NS: %u\n"
			     "\tIPv6 multicast NA: %u\n"
			     "\tICMPv4: %u\n"
			     "\tICMPv6: %u\n"
			     "\tRSSI Breach: %u\n"
			     "\tLow RSSI: %u\n"
			     "\tG-Scan: %u\n"
			     "\tPNO Complete: %u\n"
			     "\tPNO Match: %u\n"
			     "\tUC Drop wake_count: %u\n"
			     "\twake count due to fatal event: %u\n"
			     "\tOEM rsp wake_count: %u\n"
			     "\twake count due to pwr_save_fail_detected: %u\n"
			     "\twake count due to 11d scan: %u\n",
			     unspecified_wake_count,
			     wow_stats.ucast_wake_up_count,
			     wow_stats.bcast_wake_up_count,
			     wow_stats.ipv4_mcast_wake_up_count,
			     wow_stats.ipv6_mcast_wake_up_count,
			     wow_stats.ipv6_mcast_ra_stats,
			     wow_stats.ipv6_mcast_ns_stats,
			     wow_stats.ipv6_mcast_na_stats,
			     wow_stats.icmpv4_count,
			     wow_stats.icmpv6_count,
			     wow_stats.rssi_breach_wake_up_count,
			     wow_stats.low_rssi_wake_up_count,
			     wow_stats.gscan_wake_up_count,
			     wow_stats.pno_complete_wake_up_count,
			     wow_stats.pno_match_wake_up_count,
			     wow_stats.uc_drop_wake_up_count,
			     wow_stats.fatal_event_wake_up_count,
			     wow_stats.oem_response_wake_up_count,
			     wow_stats.pwr_save_fail_detected,
			     wow_stats.scan_11d);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_mc_cp_stats_send_stats_request(struct wlan_objmgr_vdev *vdev,
					       enum stats_req_type type,
					       struct request_info *info)
{
	QDF_STATUS status;

	status = ucfg_mc_cp_stats_set_pending_req(wlan_vdev_get_psoc(vdev),
						  type, info);
	if (QDF_IS_STATUS_ERROR(status)) {
		cp_stats_err("ucfg_mc_cp_stats_set_pending_req pdev failed: %d",
			     status);
		return status;
	}

	return tgt_send_mc_cp_stats_req(wlan_vdev_get_psoc(vdev), type, info);
}

#ifdef WLAN_FEATURE_BIG_DATA_STATS
QDF_STATUS ucfg_send_big_data_stats_request(struct wlan_objmgr_vdev *vdev,
					    enum stats_req_type type,
					    struct request_info *info)
{
	QDF_STATUS status;

	status = ucfg_mc_cp_stats_set_pending_req(wlan_vdev_get_psoc(vdev),
						  type, info);
	if (QDF_IS_STATUS_ERROR(status)) {
		cp_stats_err("ucfg_mc_cp_stats_set_pending_req pdev failed: %d",
			     status);
		return status;
	}
	return tgt_send_mc_cp_stats_req(wlan_vdev_get_psoc(vdev), type, info);
}
#endif

QDF_STATUS ucfg_mc_cp_stats_get_tx_power(struct wlan_objmgr_vdev *vdev,
					 int *dbm)
{
	struct wlan_objmgr_pdev *pdev;
	struct pdev_mc_cp_stats *pdev_mc_stats;
	struct pdev_cp_stats *pdev_cp_stats_priv;

	pdev = wlan_vdev_get_pdev(vdev);
	pdev_cp_stats_priv = wlan_cp_stats_get_pdev_stats_obj(pdev);
	if (!pdev_cp_stats_priv) {
		cp_stats_err("pdev cp stats object is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wlan_cp_stats_pdev_obj_lock(pdev_cp_stats_priv);
	pdev_mc_stats = pdev_cp_stats_priv->pdev_stats;
	*dbm = pdev_mc_stats->max_pwr;
	wlan_cp_stats_pdev_obj_unlock(pdev_cp_stats_priv);

	return QDF_STATUS_SUCCESS;
}

bool ucfg_mc_cp_stats_is_req_pending(struct wlan_objmgr_psoc *psoc,
				     enum stats_req_type type)
{
	uint32_t pending_req_map;
	struct psoc_mc_cp_stats *psoc_mc_stats;
	struct psoc_cp_stats *psoc_cp_stats_priv;

	psoc_cp_stats_priv = wlan_cp_stats_get_psoc_stats_obj(psoc);
	if (!psoc_cp_stats_priv) {
		cp_stats_err("psoc cp stats object is null");
		return false;
	}

	wlan_cp_stats_psoc_obj_lock(psoc_cp_stats_priv);
	psoc_mc_stats = psoc_cp_stats_priv->obj_stats;
	pending_req_map = psoc_mc_stats->pending.type_map;
	wlan_cp_stats_psoc_obj_unlock(psoc_cp_stats_priv);

	return (pending_req_map & (1 << type));
}

QDF_STATUS ucfg_mc_cp_stats_set_pending_req(struct wlan_objmgr_psoc *psoc,
					    enum stats_req_type type,
					    struct request_info *req)
{
	struct psoc_mc_cp_stats *psoc_mc_stats;
	struct psoc_cp_stats *psoc_cp_stats_priv;

	psoc_cp_stats_priv = wlan_cp_stats_get_psoc_stats_obj(psoc);
	if (!psoc_cp_stats_priv) {
		cp_stats_err("psoc cp stats object is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (type >= TYPE_MAX) {
		cp_stats_err("Invalid type index: %d", type);
		return QDF_STATUS_E_INVAL;
	}

	wlan_cp_stats_psoc_obj_lock(psoc_cp_stats_priv);
	psoc_mc_stats = psoc_cp_stats_priv->obj_stats;
	if (psoc_mc_stats->is_cp_stats_suspended) {
		cp_stats_debug("cp stats is suspended try again after resume");
		wlan_cp_stats_psoc_obj_unlock(psoc_cp_stats_priv);
		return QDF_STATUS_E_AGAIN;
	}
	psoc_mc_stats->pending.type_map |= (1 << type);
	psoc_mc_stats->pending.req[type] = *req;
	wlan_cp_stats_psoc_obj_unlock(psoc_cp_stats_priv);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_mc_cp_stats_reset_pending_req(struct wlan_objmgr_psoc *psoc,
					      enum stats_req_type type,
					      struct request_info *last_req,
					      bool *pending)
{
	struct psoc_mc_cp_stats *psoc_mc_stats;
	struct psoc_cp_stats *psoc_cp_stats_priv;

	psoc_cp_stats_priv = wlan_cp_stats_get_psoc_stats_obj(psoc);
	if (!psoc_cp_stats_priv) {
		cp_stats_err("psoc cp stats object is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (type >= TYPE_MAX) {
		cp_stats_err("Invalid type index: %d", type);
		return QDF_STATUS_E_INVAL;
	}

	wlan_cp_stats_psoc_obj_lock(psoc_cp_stats_priv);
	psoc_mc_stats = psoc_cp_stats_priv->obj_stats;
	if (psoc_mc_stats->pending.type_map & (1 << type)) {
		*last_req = psoc_mc_stats->pending.req[type];
		*pending = true;
	} else {
		*pending = false;
	}
	psoc_mc_stats->pending.type_map &= ~(1 << type);
	qdf_mem_zero(&psoc_mc_stats->pending.req[type],
		     sizeof(psoc_mc_stats->pending.req[type]));
	wlan_cp_stats_psoc_obj_unlock(psoc_cp_stats_priv);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_mc_cp_stats_get_pending_req(struct wlan_objmgr_psoc *psoc,
					    enum stats_req_type type,
					    struct request_info *info)
{
	struct psoc_mc_cp_stats *psoc_mc_stats;
	struct psoc_cp_stats *psoc_cp_stats_priv;

	psoc_cp_stats_priv = wlan_cp_stats_get_psoc_stats_obj(psoc);
	if (!psoc_cp_stats_priv) {
		cp_stats_err("psoc cp stats object is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (type >= TYPE_MAX) {
		cp_stats_err("Invalid type index: %d", type);
		return QDF_STATUS_E_INVAL;
	}
	wlan_cp_stats_psoc_obj_lock(psoc_cp_stats_priv);
	psoc_mc_stats = psoc_cp_stats_priv->obj_stats;
	*info = psoc_mc_stats->pending.req[type];
	wlan_cp_stats_psoc_obj_unlock(psoc_cp_stats_priv);

	return QDF_STATUS_SUCCESS;
}

void ucfg_mc_cp_stats_free_stats_resources(struct stats_event *ev)
{
	if (!ev)
		return;

	qdf_mem_free(ev->pdev_stats);
	qdf_mem_free(ev->peer_adv_stats);
	qdf_mem_free(ev->peer_stats);
	qdf_mem_free(ev->cca_stats);
	qdf_mem_free(ev->vdev_summary_stats);
	qdf_mem_free(ev->vdev_chain_rssi);
	qdf_mem_free(ev->peer_extended_stats);
	qdf_mem_free(ev->peer_stats_info_ext);
	qdf_mem_zero(ev, sizeof(*ev));
}

QDF_STATUS ucfg_mc_cp_stats_cca_stats_get(struct wlan_objmgr_vdev *vdev,
					  struct cca_stats *cca_stats)
{
	struct vdev_cp_stats *vdev_cp_stats_priv;
	struct vdev_mc_cp_stats *vdev_mc_stats;

	vdev_cp_stats_priv = wlan_cp_stats_get_vdev_stats_obj(vdev);
	if (!vdev_cp_stats_priv) {
		cp_stats_err("vdev cp stats object is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wlan_cp_stats_vdev_obj_lock(vdev_cp_stats_priv);
	vdev_mc_stats = vdev_cp_stats_priv->vdev_stats;
	cca_stats->congestion = vdev_mc_stats->cca.congestion;
	wlan_cp_stats_vdev_obj_unlock(vdev_cp_stats_priv);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_mc_cp_stats_set_rate_flags(struct wlan_objmgr_vdev *vdev,
					   uint32_t flags)
{
	struct vdev_mc_cp_stats *vdev_mc_stats;
	struct vdev_cp_stats *vdev_cp_stats_priv;

	vdev_cp_stats_priv = wlan_cp_stats_get_vdev_stats_obj(vdev);
	if (!vdev_cp_stats_priv) {
		cp_stats_err("vdev cp stats object is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wlan_cp_stats_vdev_obj_lock(vdev_cp_stats_priv);
	vdev_mc_stats = vdev_cp_stats_priv->vdev_stats;
	vdev_mc_stats->tx_rate_flags = flags;
	wlan_cp_stats_vdev_obj_unlock(vdev_cp_stats_priv);

	return QDF_STATUS_SUCCESS;
}

void ucfg_mc_cp_stats_register_lost_link_info_cb(
			struct wlan_objmgr_psoc *psoc,
			void (*lost_link_cp_stats_info_cb)(void *stats_ev))
{
	struct psoc_cp_stats *psoc_cp_stats_priv;

	psoc_cp_stats_priv = wlan_cp_stats_get_psoc_stats_obj(psoc);
	if (!psoc_cp_stats_priv) {
		cp_stats_err("psoc cp stats object is null");
		return;
	}

	psoc_cp_stats_priv->legacy_stats_cb = lost_link_cp_stats_info_cb;
}

#ifdef WLAN_POWER_MANAGEMENT_OFFLOAD
static QDF_STATUS
ucfg_mc_cp_stats_suspend_req_handler(struct wlan_objmgr_psoc *psoc)
{
	struct psoc_mc_cp_stats *psoc_mc_stats;
	struct psoc_cp_stats *psoc_cp_stats_priv;

	psoc_cp_stats_priv = wlan_cp_stats_get_psoc_stats_obj(psoc);
	if (!psoc_cp_stats_priv) {
		cp_stats_err("psoc cp stats object is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wlan_cp_stats_psoc_obj_lock(psoc_cp_stats_priv);
	psoc_mc_stats = psoc_cp_stats_priv->obj_stats;
	psoc_mc_stats->is_cp_stats_suspended = true;
	wlan_cp_stats_psoc_obj_unlock(psoc_cp_stats_priv);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
ucfg_mc_cp_stats_resume_req_handler(struct wlan_objmgr_psoc *psoc)
{
	struct psoc_mc_cp_stats *psoc_mc_stats;
	struct psoc_cp_stats *psoc_cp_stats_priv;

	psoc_cp_stats_priv = wlan_cp_stats_get_psoc_stats_obj(psoc);
	if (!psoc_cp_stats_priv) {
		cp_stats_err("psoc cp stats object is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wlan_cp_stats_psoc_obj_lock(psoc_cp_stats_priv);
	psoc_mc_stats = psoc_cp_stats_priv->obj_stats;
	psoc_mc_stats->is_cp_stats_suspended = false;
	wlan_cp_stats_psoc_obj_unlock(psoc_cp_stats_priv);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
ucfg_mc_cp_stats_resume_handler(struct wlan_objmgr_psoc *psoc,
				void *arg)
{
	return ucfg_mc_cp_stats_resume_req_handler(psoc);
}

static QDF_STATUS
ucfg_mc_cp_stats_suspend_handler(struct wlan_objmgr_psoc *psoc,
				 void *arg)
{
	return ucfg_mc_cp_stats_suspend_req_handler(psoc);
}

void ucfg_mc_cp_stats_register_pmo_handler(void)
{
	pmo_register_suspend_handler(WLAN_UMAC_COMP_CP_STATS,
				     ucfg_mc_cp_stats_suspend_handler, NULL);
	pmo_register_resume_handler(WLAN_UMAC_COMP_CP_STATS,
				    ucfg_mc_cp_stats_resume_handler, NULL);
}
#endif
