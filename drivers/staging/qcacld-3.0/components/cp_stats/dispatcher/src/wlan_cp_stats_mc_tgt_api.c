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
 * DOC:wlan_cp_stats_mc_tgt_api.c
 *
 * This file provide API definitions to update control plane statistics received
 * from southbound interface
 */

#include "wlan_cp_stats_mc_defs.h"
#include "target_if_cp_stats.h"
#include "wlan_cp_stats_tgt_api.h"
#include "wlan_cp_stats_ucfg_api.h"
#include "wlan_cp_stats_mc_tgt_api.h"
#include <wlan_cp_stats_mc_ucfg_api.h>
#include <wlan_cp_stats_utils_api.h>
#include "../../core/src/wlan_cp_stats_defs.h"
#include "../../core/src/wlan_cp_stats_obj_mgr_handler.h"

static bool tgt_mc_cp_stats_is_last_event(struct stats_event *ev,
					  enum stats_req_type stats_type)
{
	bool is_last_event;

	if (IS_MSB_SET(ev->last_event)) {
		is_last_event = IS_LSB_SET(ev->last_event);
	} else {
		if (stats_type == TYPE_CONNECTION_TX_POWER)
			is_last_event = true;
		else
			is_last_event = !!ev->peer_stats;
	}

	if (is_last_event)
		cp_stats_debug("Last stats event");

	return is_last_event;
}

#ifdef WLAN_FEATURE_BIG_DATA_STATS
static void
tgt_cp_stats_register_big_data_rx_ops(struct wlan_lmac_if_rx_ops *rx_ops)
{
	rx_ops->cp_stats_rx_ops.process_big_data_stats_event =
		tgt_mc_cp_stats_process_big_data_stats_event;
}

static QDF_STATUS
send_big_data_stats_req(struct wlan_lmac_if_cp_stats_tx_ops *tx_ops,
			struct wlan_objmgr_psoc *psoc,
			struct request_info *req)
{
	if (!tx_ops->send_req_big_data_stats) {
		cp_stats_err("could not get send_req_big_data_stats");
		return QDF_STATUS_E_NULL_VALUE;
	}

	return tx_ops->send_req_big_data_stats(psoc, req);
}
#else
static void
tgt_cp_stats_register_big_data_rx_ops(struct wlan_lmac_if_rx_ops *rx_ops)
{}

static QDF_STATUS
send_big_data_stats_req(struct wlan_lmac_if_cp_stats_tx_ops *tx_ops,
			struct wlan_objmgr_psoc *psoc,
			struct request_info *req)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_SUPPORT_INFRA_CTRL_PATH_STATS
static void
tgt_cp_stats_register_infra_cp_stats_rx_ops(struct wlan_lmac_if_rx_ops *rx_ops)
{
	rx_ops->cp_stats_rx_ops.process_infra_stats_event =
				tgt_mc_cp_stats_process_infra_stats_event;
}
#else
static void
tgt_cp_stats_register_infra_cp_stats_rx_ops(struct wlan_lmac_if_rx_ops *rx_ops)
{
}
#endif

void tgt_cp_stats_register_rx_ops(struct wlan_lmac_if_rx_ops *rx_ops)
{
	rx_ops->cp_stats_rx_ops.process_stats_event =
					tgt_mc_cp_stats_process_stats_event;
	tgt_cp_stats_register_infra_cp_stats_rx_ops(rx_ops);
	tgt_cp_stats_register_big_data_rx_ops(rx_ops);
}

static void tgt_mc_cp_stats_extract_tx_power(struct wlan_objmgr_psoc *psoc,
					     struct stats_event *ev,
					     bool is_station_stats)
{
	int32_t max_pwr;
	uint8_t pdev_id;
	QDF_STATUS status;
	struct wlan_objmgr_pdev *pdev;
	struct request_info last_req = {0};
	struct wlan_objmgr_vdev *vdev = NULL;
	struct pdev_mc_cp_stats *pdev_mc_stats;
	struct pdev_cp_stats *pdev_cp_stats_priv;
	bool pending = false;

	if (!ev->pdev_stats)
		return;

	if (is_station_stats)
		status = ucfg_mc_cp_stats_get_pending_req(psoc,
					TYPE_STATION_STATS, &last_req);
	else
		status = ucfg_mc_cp_stats_get_pending_req(psoc,
					TYPE_CONNECTION_TX_POWER, &last_req);

	if (QDF_IS_STATUS_ERROR(status)) {
		cp_stats_err("ucfg_mc_cp_stats_get_pending_req failed");
		goto end;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, last_req.vdev_id,
						    WLAN_CP_STATS_ID);
	if (!vdev) {
		cp_stats_err("vdev is null");
		goto end;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		cp_stats_err("pdev is null");
		goto end;
	}

	pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);
	if (pdev_id >= ev->num_pdev_stats) {
		cp_stats_err("pdev_id: %d invalid", pdev_id);
		goto end;
	}

	pdev_cp_stats_priv = wlan_cp_stats_get_pdev_stats_obj(pdev);
	if (!pdev_cp_stats_priv) {
		cp_stats_err("pdev_cp_stats_priv is null");
		goto end;
	}

	wlan_cp_stats_pdev_obj_lock(pdev_cp_stats_priv);
	pdev_mc_stats = pdev_cp_stats_priv->pdev_stats;
	max_pwr = pdev_mc_stats->max_pwr = ev->pdev_stats[pdev_id].max_pwr;
	wlan_cp_stats_pdev_obj_unlock(pdev_cp_stats_priv);
	if (is_station_stats)
		goto end;

	if (tgt_mc_cp_stats_is_last_event(ev, TYPE_CONNECTION_TX_POWER)) {
		ucfg_mc_cp_stats_reset_pending_req(psoc,
						   TYPE_CONNECTION_TX_POWER,
						   &last_req,
						   &pending);
		if (last_req.u.get_tx_power_cb && pending)
			last_req.u.get_tx_power_cb(max_pwr, last_req.cookie);
	}
end:
	if (vdev)
		wlan_objmgr_vdev_release_ref(vdev, WLAN_CP_STATS_ID);
}

static void peer_rssi_iterator(struct wlan_objmgr_pdev *pdev,
			       void *peer, void *arg)
{
	struct stats_event *ev;
	struct peer_mc_cp_stats *peer_mc_stats;
	struct peer_cp_stats *peer_cp_stats_priv;
	struct peer_extd_stats *peer_extd_mc_stats;

	if (WLAN_PEER_SELF == wlan_peer_get_peer_type(peer)) {
		cp_stats_debug("ignore self peer: "QDF_MAC_ADDR_FMT,
			       QDF_MAC_ADDR_REF(wlan_peer_get_macaddr(peer)));
		return;
	}

	peer_cp_stats_priv = wlan_cp_stats_get_peer_stats_obj(peer);
	if (!peer_cp_stats_priv) {
		cp_stats_err("peer cp stats object is null");
		return;
	}

	wlan_cp_stats_peer_obj_lock(peer_cp_stats_priv);
	peer_mc_stats = peer_cp_stats_priv->peer_stats;
	ev = arg;
	ev->peer_stats[ev->num_peer_stats] = *peer_mc_stats;
	ev->num_peer_stats++;

	peer_extd_mc_stats = peer_mc_stats->extd_stats;
	ev->peer_extended_stats[ev->num_peer_extd_stats] = *peer_extd_mc_stats;
	ev->num_peer_extd_stats++;
	wlan_cp_stats_peer_obj_unlock(peer_cp_stats_priv);
}

static void
tgt_mc_cp_stats_prepare_raw_peer_rssi(struct wlan_objmgr_psoc *psoc,
				      struct request_info *last_req)
{
	uint8_t *mac_addr;
	uint16_t peer_count;
	struct stats_event ev = {0};
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_peer *peer = NULL;
	struct peer_mc_cp_stats *peer_mc_stats;
	struct peer_extd_stats *peer_mc_extd_stats;
	struct peer_cp_stats *peer_cp_stats_priv;
	void (*get_peer_rssi_cb)(struct stats_event *ev, void *cookie);

	get_peer_rssi_cb = last_req->u.get_peer_rssi_cb;
	if (!get_peer_rssi_cb) {
		cp_stats_debug("get_peer_rssi_cb is null");
		return;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, last_req->vdev_id,
						    WLAN_CP_STATS_ID);
	if (!vdev) {
		cp_stats_err("vdev is null");
		goto end;
	}

	mac_addr = last_req->peer_mac_addr;
	if (QDF_IS_ADDR_BROADCAST(mac_addr)) {
		pdev = wlan_vdev_get_pdev(vdev);
		peer_count = wlan_pdev_get_peer_count(pdev);
		ev.peer_stats = qdf_mem_malloc(sizeof(*ev.peer_stats) *
								peer_count);
		if (!ev.peer_stats)
			goto end;

		ev.peer_extended_stats =
			qdf_mem_malloc(sizeof(*ev.peer_extended_stats) *
				       peer_count);
		if (!ev.peer_extended_stats)
			goto end;

		wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_PEER_OP,
						  peer_rssi_iterator, &ev,
						  true, WLAN_CP_STATS_ID);
	} else {
		peer = wlan_objmgr_get_peer(psoc, last_req->pdev_id,
					    mac_addr, WLAN_CP_STATS_ID);
		if (!peer) {
			cp_stats_debug("peer[" QDF_MAC_ADDR_FMT "] is null",
				       QDF_MAC_ADDR_REF(mac_addr));
			goto end;
		}

		peer_cp_stats_priv = wlan_cp_stats_get_peer_stats_obj(peer);
		if (!peer_cp_stats_priv) {
			cp_stats_err("peer cp stats object is null");
			goto end;
		}

		ev.peer_stats = qdf_mem_malloc(sizeof(*ev.peer_stats));
		if (!ev.peer_stats)
			goto end;

		ev.num_peer_stats = 1;

		ev.peer_extended_stats =
			qdf_mem_malloc(sizeof(*ev.peer_extended_stats));
		if (!ev.peer_extended_stats)
			goto end;

		ev.num_peer_extd_stats = 1;

		wlan_cp_stats_peer_obj_lock(peer_cp_stats_priv);
		peer_mc_stats = peer_cp_stats_priv->peer_stats;
		*ev.peer_stats = *peer_mc_stats;

		peer_mc_extd_stats = peer_mc_stats->extd_stats;
		*ev.peer_extended_stats = *peer_mc_extd_stats;
		wlan_cp_stats_peer_obj_unlock(peer_cp_stats_priv);
	}

end:
	if (ev.peer_stats)
		get_peer_rssi_cb(&ev, last_req->cookie);

	ucfg_mc_cp_stats_free_stats_resources(&ev);

	if (vdev)
		wlan_objmgr_vdev_release_ref(vdev, WLAN_CP_STATS_ID);
	if (peer)
		wlan_objmgr_peer_release_ref(peer, WLAN_CP_STATS_ID);
}

static QDF_STATUS
tgt_mc_cp_stats_update_peer_adv_stats(struct wlan_objmgr_psoc *psoc,
				      struct peer_adv_mc_cp_stats
				      *peer_adv_stats, uint32_t size)
{
	uint8_t *peer_mac_addr;
	struct wlan_objmgr_peer *peer;
	struct peer_mc_cp_stats *peer_mc_stats;
	struct peer_adv_mc_cp_stats *peer_adv_mc_stats;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct peer_cp_stats *peer_cp_stats_priv;

	if (!peer_adv_stats)
		return QDF_STATUS_E_INVAL;

	peer_mac_addr = peer_adv_stats->peer_macaddr;
	peer = wlan_objmgr_get_peer_by_mac(psoc, peer_mac_addr,
					   WLAN_CP_STATS_ID);
	if (!peer) {
		cp_stats_debug("peer is null");
		return QDF_STATUS_E_EXISTS;
	}
	peer_cp_stats_priv = wlan_cp_stats_get_peer_stats_obj(peer);
	if (!peer_cp_stats_priv) {
		cp_stats_err("peer_cp_stats_priv is null");
		status = QDF_STATUS_E_EXISTS;
		goto end;
	}
	wlan_cp_stats_peer_obj_lock(peer_cp_stats_priv);
	peer_mc_stats = peer_cp_stats_priv->peer_stats;
	peer_adv_mc_stats = peer_mc_stats->adv_stats;

	qdf_mem_copy(peer_adv_mc_stats->peer_macaddr,
		     peer_adv_stats->peer_macaddr,
		     QDF_MAC_ADDR_SIZE);
	if (peer_adv_stats->fcs_count)
		peer_adv_mc_stats->fcs_count = peer_adv_stats->fcs_count;
	if (peer_adv_stats->rx_bytes)
		peer_adv_mc_stats->rx_bytes = peer_adv_stats->rx_bytes;
	if (peer_adv_stats->rx_count)
		peer_adv_mc_stats->rx_count = peer_adv_stats->rx_count;
	wlan_cp_stats_peer_obj_unlock(peer_cp_stats_priv);

end:
	if (peer)
		wlan_objmgr_peer_release_ref(peer, WLAN_CP_STATS_ID);

	return status;
}

static QDF_STATUS
tgt_mc_cp_stats_update_peer_stats(struct wlan_objmgr_psoc *psoc,
				  struct peer_mc_cp_stats *peer_stats)
{
	uint8_t *peer_mac_addr;
	struct wlan_objmgr_peer *peer;
	struct peer_mc_cp_stats *peer_mc_stats;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct peer_cp_stats *peer_cp_stats_priv;

	if (!peer_stats)
		return QDF_STATUS_E_INVAL;

	peer_mac_addr = peer_stats->peer_macaddr;
	peer = wlan_objmgr_get_peer_by_mac(psoc, peer_mac_addr,
				    WLAN_CP_STATS_ID);
	if (!peer) {
		cp_stats_debug("peer is null");
		return QDF_STATUS_E_EXISTS;
	}

	peer_cp_stats_priv = wlan_cp_stats_get_peer_stats_obj(peer);
	if (!peer_cp_stats_priv) {
		cp_stats_err("peer_cp_stats_priv is null");
		status = QDF_STATUS_E_EXISTS;
		goto end;
	}

	wlan_cp_stats_peer_obj_lock(peer_cp_stats_priv);
	peer_mc_stats = peer_cp_stats_priv->peer_stats;
	qdf_mem_copy(peer_mc_stats->peer_macaddr,
		     peer_stats->peer_macaddr,
		     QDF_MAC_ADDR_SIZE);
	if (peer_stats->tx_rate)
		peer_mc_stats->tx_rate = peer_stats->tx_rate;
	if (peer_stats->rx_rate)
		peer_mc_stats->rx_rate = peer_stats->rx_rate;
	if (peer_stats->peer_rssi)
		peer_mc_stats->peer_rssi = peer_stats->peer_rssi;
	cp_stats_nofl_debug("PEER STATS: peer_mac="QDF_MAC_ADDR_FMT", tx_rate=%u, rx_rate=%u, peer_rssi=%d",
			    QDF_MAC_ADDR_REF(peer_mc_stats->peer_macaddr),
			    peer_mc_stats->tx_rate,
			    peer_mc_stats->rx_rate, peer_mc_stats->peer_rssi);
	wlan_cp_stats_peer_obj_unlock(peer_cp_stats_priv);

end:
	if (peer)
		wlan_objmgr_peer_release_ref(peer, WLAN_CP_STATS_ID);

	return status;
}

static QDF_STATUS
tgt_mc_cp_stats_update_peer_extd_stats(
				struct wlan_objmgr_psoc *psoc,
				struct peer_extd_stats *peer_extended_stats)
{
	uint8_t *peer_mac_addr;
	struct wlan_objmgr_peer *peer;
	struct peer_mc_cp_stats *peer_mc_stats;
	struct peer_extd_stats *peer_extd_mc_stats;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct peer_cp_stats *peer_cp_stats_priv;

	if (!peer_extended_stats)
		return QDF_STATUS_E_INVAL;

	peer_mac_addr = peer_extended_stats->peer_macaddr;
	peer = wlan_objmgr_get_peer_by_mac(psoc, peer_mac_addr,
					   WLAN_CP_STATS_ID);
	if (!peer) {
		cp_stats_debug("peer is null");
		return QDF_STATUS_E_EXISTS;
	}

	peer_cp_stats_priv = wlan_cp_stats_get_peer_stats_obj(peer);
	if (!peer_cp_stats_priv) {
		cp_stats_err("peer_cp_stats_priv is null");
		status = QDF_STATUS_E_EXISTS;
		goto end;
	}

	wlan_cp_stats_peer_obj_lock(peer_cp_stats_priv);
	peer_mc_stats = peer_cp_stats_priv->peer_stats;
	peer_extd_mc_stats = peer_mc_stats->extd_stats;
	if (!peer_extd_mc_stats) {
		wlan_cp_stats_peer_obj_unlock(peer_cp_stats_priv);
		cp_stats_err("No peer_extd_mc_stats");
		status = QDF_STATUS_E_INVAL;
		goto end;
	}
	qdf_mem_copy(peer_extd_mc_stats->peer_macaddr,
		     peer_extended_stats->peer_macaddr,
		     QDF_MAC_ADDR_SIZE);
	if (peer_extended_stats->rx_mc_bc_cnt)
		peer_extd_mc_stats->rx_mc_bc_cnt =
					peer_extended_stats->rx_mc_bc_cnt;
	wlan_cp_stats_peer_obj_unlock(peer_cp_stats_priv);

	cp_stats_debug("peer_mac="QDF_MAC_ADDR_FMT", rx_mc_bc_cnt=%u",
		       QDF_MAC_ADDR_REF(peer_extended_stats->peer_macaddr),
		       peer_extended_stats->rx_mc_bc_cnt);

end:
		wlan_objmgr_peer_release_ref(peer, WLAN_CP_STATS_ID);

	return status;
}

static void tgt_mc_cp_stats_extract_peer_extd_stats(
						struct wlan_objmgr_psoc *psoc,
						struct stats_event *ev)
{
	uint32_t i, selected;
	QDF_STATUS status;
	struct request_info last_req = {0};

	status = ucfg_mc_cp_stats_get_pending_req(psoc,
						  TYPE_PEER_STATS,
						  &last_req);

	if (QDF_IS_STATUS_ERROR(status)) {
		cp_stats_err("ucfg_mc_cp_stats_get_pending_req failed");
		return;
	}

	selected = ev->num_peer_extd_stats;
	for (i = 0; i < ev->num_peer_extd_stats; i++) {
		status = tgt_mc_cp_stats_update_peer_extd_stats(
						psoc,
						&ev->peer_extended_stats[i]);

		if (!QDF_IS_ADDR_BROADCAST(last_req.peer_mac_addr) &&
		    !qdf_mem_cmp(ev->peer_extended_stats[i].peer_macaddr,
				 last_req.peer_mac_addr,
				 QDF_MAC_ADDR_SIZE)) {
			/* mac is specified, but failed to update the peer */
			if (QDF_IS_STATUS_ERROR(status))
				return;

			selected = i;
		}
	}

	/* no matched peer */
	if (!QDF_IS_ADDR_BROADCAST(last_req.peer_mac_addr) &&
	    selected == ev->num_peer_extd_stats) {
		cp_stats_rl_err("peer not found stats");
		return;
	}
}

static void tgt_mc_cp_stats_extract_peer_stats(struct wlan_objmgr_psoc *psoc,
					       struct stats_event *ev,
					       bool is_station_stats)
{
	uint32_t i;
	QDF_STATUS status;
	struct request_info last_req = {0};
	bool pending = false;
	uint32_t selected;

	if (is_station_stats)
		status = ucfg_mc_cp_stats_get_pending_req(psoc,
							  TYPE_STATION_STATS,
							  &last_req);
	else
		status = ucfg_mc_cp_stats_get_pending_req(psoc,
							  TYPE_PEER_STATS,
							  &last_req);

	if (QDF_IS_STATUS_ERROR(status)) {
		cp_stats_err("ucfg_mc_cp_stats_get_pending_req failed");
		return;
	}

	if (!ev->peer_stats)
		goto extd2_stats;

	selected = ev->num_peer_stats;
	for (i = 0; i < ev->num_peer_stats; i++) {
		status = tgt_mc_cp_stats_update_peer_stats(psoc,
							   &ev->peer_stats[i]);
		if (!QDF_IS_ADDR_BROADCAST(last_req.peer_mac_addr) &&
		    !qdf_mem_cmp(ev->peer_stats[i].peer_macaddr,
				 last_req.peer_mac_addr,
				 QDF_MAC_ADDR_SIZE)) {
			/* mac is specified, but failed to update the peer */
			if (QDF_IS_STATUS_ERROR(status))
				return;

			selected = i;
		}
	}

	/* no matched peer */
	if (!QDF_IS_ADDR_BROADCAST(last_req.peer_mac_addr) &&
	    selected == ev->num_peer_stats)
		cp_stats_debug("peer not found for stats");

extd2_stats:

	if (!ev->peer_adv_stats)
		goto complete;

	selected = ev->num_peer_adv_stats;
	for (i = 0; i < ev->num_peer_adv_stats; i++) {
		status = tgt_mc_cp_stats_update_peer_adv_stats(
						psoc, &ev->peer_adv_stats[i],
						ev->num_peer_adv_stats);
		if (!QDF_IS_ADDR_BROADCAST(last_req.peer_mac_addr) &&
		    !qdf_mem_cmp(ev->peer_adv_stats[i].peer_macaddr,
				 last_req.peer_mac_addr,
				 QDF_MAC_ADDR_SIZE)) {
			/* mac is specified, but failed to update the peer */
			if (QDF_IS_STATUS_ERROR(status))
				return;

			selected = i;
		}
	}

	/* no matched peer */
	if (!QDF_IS_ADDR_BROADCAST(last_req.peer_mac_addr) &&
	    selected == ev->num_peer_adv_stats) {
		cp_stats_debug("peer not found for extd stats");
		return;
	}

complete:
	if (is_station_stats)
		return;

	tgt_mc_cp_stats_extract_peer_extd_stats(psoc, ev);
	if (tgt_mc_cp_stats_is_last_event(ev, TYPE_PEER_STATS)) {
		ucfg_mc_cp_stats_reset_pending_req(psoc, TYPE_PEER_STATS,
						   &last_req, &pending);
		if (pending && last_req.u.get_peer_rssi_cb)
			tgt_mc_cp_stats_prepare_raw_peer_rssi(psoc, &last_req);
	}
}

#ifdef WLAN_FEATURE_MIB_STATS
static void tgt_mc_cp_stats_extract_mib_stats(struct wlan_objmgr_psoc *psoc,
					      struct stats_event *ev)
{
	QDF_STATUS status;
	struct request_info last_req = {0};
	bool pending = false;

	if (!ev->mib_stats) {
		cp_stats_debug("no mib stats");
		return;
	}

	status = ucfg_mc_cp_stats_get_pending_req(psoc,
						  TYPE_MIB_STATS, &last_req);

	if (QDF_IS_STATUS_ERROR(status)) {
		cp_stats_err("ucfg_mc_cp_stats_get_pending_req failed");
		return;
	}

	if (tgt_mc_cp_stats_is_last_event(ev, TYPE_MIB_STATS)) {
		ucfg_mc_cp_stats_reset_pending_req(psoc, TYPE_MIB_STATS,
						   &last_req, &pending);
		if (last_req.u.get_mib_stats_cb && pending)
			last_req.u.get_mib_stats_cb(ev, last_req.cookie);
	}
}
#else
static void tgt_mc_cp_stats_extract_mib_stats(struct wlan_objmgr_psoc *psoc,
					      struct stats_event *ev)
{
}
#endif

static void
tgt_mc_cp_stats_extract_peer_stats_info_ext(struct wlan_objmgr_psoc *psoc,
					    struct stats_event *ev)
{
	QDF_STATUS status;
	struct request_info last_req = {0};
	bool pending = false;

	if (!ev->peer_stats_info_ext || ev->num_peer_stats_info_ext == 0) {
		cp_stats_debug("no peer_stats_info_ext");
		return;
	}

	status = ucfg_mc_cp_stats_get_pending_req(psoc,
						  TYPE_PEER_STATS_INFO_EXT,
						  &last_req);
	if (QDF_IS_STATUS_ERROR(status)) {
		cp_stats_err("ucfg_mc_cp_stats_get_pending_req failed");
		return;
	}

	ucfg_mc_cp_stats_reset_pending_req(psoc, TYPE_PEER_STATS_INFO_EXT,
					   &last_req, &pending);
	if (last_req.u.get_peer_stats_cb && pending) {
		last_req.u.get_peer_stats_cb(ev, last_req.cookie);
		last_req.u.get_peer_stats_cb = NULL;
	}
}

#ifdef WLAN_SUPPORT_INFRA_CTRL_PATH_STATS
#ifdef WLAN_SUPPORT_TWT
static void
tgt_mc_infra_cp_stats_extract_twt_stats(struct wlan_objmgr_psoc *psoc,
					struct infra_cp_stats_event *ev)
{
	QDF_STATUS status;
	get_infra_cp_stats_cb resp_cb;
	void *context;

	status = wlan_cp_stats_infra_cp_get_context(psoc, &resp_cb, &context);
	if (QDF_IS_STATUS_ERROR(status)) {
		cp_stats_err("ucfg_get_infra_cp_stats_context failed");
		return;
	}

	cp_stats_debug("num_twt_infra_cp_stats = %d action %d",
		       ev->num_twt_infra_cp_stats, ev->action);

	if (resp_cb)
		resp_cb(ev, context);
}
#else
static void
tgt_mc_infra_cp_stats_extract_twt_stats(struct wlan_objmgr_psoc *psoc,
					struct infra_cp_stats_event *ev)
{
}
#endif
#endif /* WLAN_SUPPORT_INFRA_CTRL_PATH_STATS */

static void tgt_mc_cp_stats_extract_cca_stats(struct wlan_objmgr_psoc *psoc,
						  struct stats_event *ev)
{
	struct wlan_objmgr_vdev *vdev;
	struct vdev_mc_cp_stats *vdev_mc_stats;
	struct vdev_cp_stats *vdev_cp_stats_priv;

	if (!ev->cca_stats)
		return;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
						    ev->cca_stats->vdev_id,
						    WLAN_CP_STATS_ID);
	if (!vdev) {
		cp_stats_err("vdev is null");
		return;
	}

	vdev_cp_stats_priv = wlan_cp_stats_get_vdev_stats_obj(vdev);
	if (!vdev_cp_stats_priv) {
		cp_stats_err("vdev cp stats object is null");
		goto end;
	}

	wlan_cp_stats_vdev_obj_lock(vdev_cp_stats_priv);
	vdev_mc_stats = vdev_cp_stats_priv->vdev_stats;
	vdev_mc_stats->cca.congestion =  ev->cca_stats->congestion;
	wlan_cp_stats_vdev_obj_unlock(vdev_cp_stats_priv);

end:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_CP_STATS_ID);
}

static void
tgt_mc_cp_stats_extract_pmf_bcn_stats(struct wlan_objmgr_psoc *psoc,
				      struct stats_event *ev)
{
	QDF_STATUS status;
	struct request_info last_req = {0};
	struct wlan_objmgr_vdev *vdev;
	struct vdev_mc_cp_stats *vdev_mc_stats;
	struct vdev_cp_stats *vdev_cp_stats_priv;

	status = ucfg_mc_cp_stats_get_pending_req(psoc,
						  TYPE_STATION_STATS,
						  &last_req);
	if (QDF_IS_STATUS_ERROR(status)) {
		cp_stats_err("ucfg_mc_cp_stats_get_pending_req failed");
		return;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, last_req.vdev_id,
						    WLAN_CP_STATS_ID);
	if (!vdev) {
		cp_stats_err("vdev is null");
		return;
	}

	vdev_cp_stats_priv = wlan_cp_stats_get_vdev_stats_obj(vdev);
	if (!vdev_cp_stats_priv) {
		cp_stats_err("vdev cp stats object is null");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_CP_STATS_ID);
		return;
	}

	wlan_cp_stats_vdev_obj_lock(vdev_cp_stats_priv);
	vdev_mc_stats = vdev_cp_stats_priv->vdev_stats;

	if (ev->bcn_protect_stats.pmf_bcn_stats_valid)
		vdev_mc_stats->pmf_bcn_stats = ev->bcn_protect_stats;

	wlan_cp_stats_vdev_obj_unlock(vdev_cp_stats_priv);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_CP_STATS_ID);
}

static void tgt_mc_cp_stats_extract_vdev_summary_stats(
					struct wlan_objmgr_psoc *psoc,
					struct stats_event *ev)
{
	uint8_t i;
	QDF_STATUS status;
	struct wlan_objmgr_peer *peer = NULL;
	struct request_info last_req = {0};
	struct wlan_objmgr_vdev *vdev;
	struct peer_mc_cp_stats *peer_mc_stats;
	struct vdev_mc_cp_stats *vdev_mc_stats;
	struct peer_cp_stats *peer_cp_stats_priv;
	struct vdev_cp_stats *vdev_cp_stats_priv;

	if (!ev->vdev_summary_stats)
		return;

	status = ucfg_mc_cp_stats_get_pending_req(psoc,
						 TYPE_STATION_STATS,
						 &last_req);
	if (QDF_IS_STATUS_ERROR(status)) {
		cp_stats_err("ucfg_mc_cp_stats_get_pending_req failed");
		return;
	}

	for (i = 0; i < ev->num_summary_stats; i++) {
		if (ev->vdev_summary_stats[i].vdev_id == last_req.vdev_id)
			break;
	}

	if (i == ev->num_summary_stats) {
		cp_stats_debug("vdev_id %d not found", last_req.vdev_id);
		return;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, last_req.vdev_id,
						    WLAN_CP_STATS_ID);
	if (!vdev) {
		cp_stats_err("vdev is null");
		return;
	}

	vdev_cp_stats_priv = wlan_cp_stats_get_vdev_stats_obj(vdev);
	if (!vdev_cp_stats_priv) {
		cp_stats_err("vdev cp stats object is null");
		goto end;
	}

	wlan_cp_stats_vdev_obj_lock(vdev_cp_stats_priv);
	vdev_mc_stats = vdev_cp_stats_priv->vdev_stats;
	qdf_mem_copy(&vdev_mc_stats->vdev_summary_stats,
		     &ev->vdev_summary_stats[i].stats,
		     sizeof(vdev_mc_stats->vdev_summary_stats));

	wlan_cp_stats_vdev_obj_unlock(vdev_cp_stats_priv);

	peer = wlan_objmgr_get_peer(psoc, last_req.pdev_id,
				    last_req.peer_mac_addr, WLAN_CP_STATS_ID);
	if (!peer) {
		cp_stats_debug("peer is null "QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(last_req.peer_mac_addr));
		goto end;
	}

	peer_cp_stats_priv = wlan_cp_stats_get_peer_stats_obj(peer);
	if (!peer_cp_stats_priv) {
		cp_stats_err("peer cp stats object is null");
		goto end;
	}

	wlan_cp_stats_peer_obj_lock(peer_cp_stats_priv);
	peer_mc_stats = peer_cp_stats_priv->peer_stats;
	peer_mc_stats->peer_rssi = ev->vdev_summary_stats[i].stats.rssi;
	wlan_cp_stats_peer_obj_unlock(peer_cp_stats_priv);

end:
	if (peer)
		wlan_objmgr_peer_release_ref(peer, WLAN_CP_STATS_ID);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_CP_STATS_ID);
}

static void tgt_mc_cp_stats_extract_vdev_chain_rssi_stats(
					struct wlan_objmgr_psoc *psoc,
					struct stats_event *ev)
{
	uint8_t i, j;
	QDF_STATUS status;
	struct request_info last_req = {0};
	struct wlan_objmgr_vdev *vdev;
	struct vdev_mc_cp_stats *vdev_mc_stats;
	struct vdev_cp_stats *vdev_cp_stats_priv;

	if (!ev->vdev_chain_rssi)
		return;

	status = ucfg_mc_cp_stats_get_pending_req(psoc,
						  TYPE_STATION_STATS,
						  &last_req);
	if (QDF_IS_STATUS_ERROR(status)) {
		cp_stats_err("ucfg_mc_cp_stats_get_pending_req failed");
		return;
	}

	for (i = 0; i < ev->num_chain_rssi_stats; i++) {
		if (ev->vdev_chain_rssi[i].vdev_id == last_req.vdev_id)
			break;
	}

	if (i == ev->num_chain_rssi_stats) {
		cp_stats_debug("vdev_id %d not found", last_req.vdev_id);
		return;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, last_req.vdev_id,
						    WLAN_CP_STATS_ID);
	if (!vdev) {
		cp_stats_err("vdev is null");
		return;
	}

	vdev_cp_stats_priv = wlan_cp_stats_get_vdev_stats_obj(vdev);
	if (!vdev_cp_stats_priv) {
		cp_stats_err("vdev cp stats object is null");
		goto end;
	}

	wlan_cp_stats_vdev_obj_lock(vdev_cp_stats_priv);
	vdev_mc_stats = vdev_cp_stats_priv->vdev_stats;
	for (j = 0; j < MAX_NUM_CHAINS; j++) {
		vdev_mc_stats->chain_rssi[j] =
					ev->vdev_chain_rssi[i].chain_rssi[j];
	}
	wlan_cp_stats_vdev_obj_unlock(vdev_cp_stats_priv);

end:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_CP_STATS_ID);
}

static void
tgt_mc_cp_stats_prepare_n_send_raw_station_stats(struct wlan_objmgr_psoc *psoc,
						 struct request_info *last_req)
{
	/* station_stats to be given to userspace thread */
	struct stats_event info = {0};
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_peer *peer;
	struct peer_mc_cp_stats *peer_mc_stats;
	struct vdev_mc_cp_stats *vdev_mc_stats;
	struct peer_cp_stats *peer_cp_stats_priv;
	struct vdev_cp_stats *vdev_cp_stats_priv;
	void (*get_station_stats_cb)(struct stats_event *info, void *cookie);

	get_station_stats_cb = last_req->u.get_station_stats_cb;
	if (!get_station_stats_cb) {
		cp_stats_err("callback is null");
		return;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, last_req->vdev_id,
						    WLAN_CP_STATS_ID);
	if (!vdev) {
		cp_stats_err("vdev object is null");
		return;
	}

	peer = wlan_objmgr_get_peer(psoc, last_req->pdev_id,
				    last_req->peer_mac_addr, WLAN_CP_STATS_ID);
	if (!peer) {
		cp_stats_err("peer object is null");
		goto end;
	}

	vdev_cp_stats_priv = wlan_cp_stats_get_vdev_stats_obj(vdev);
	if (!vdev_cp_stats_priv) {
		cp_stats_err("vdev cp stats object is null");
		goto end;
	}

	peer_cp_stats_priv = wlan_cp_stats_get_peer_stats_obj(peer);
	if (!peer_cp_stats_priv) {
		cp_stats_err("peer cp stats object is null");
		goto end;
	}

	info.num_summary_stats = 1;
	info.vdev_summary_stats = qdf_mem_malloc(
					sizeof(*info.vdev_summary_stats));
	if (!info.vdev_summary_stats)
		goto end;

	info.num_chain_rssi_stats = 1;
	info.vdev_chain_rssi = qdf_mem_malloc(sizeof(*info.vdev_chain_rssi));;
	if (!info.vdev_chain_rssi)
		goto end;

	wlan_cp_stats_vdev_obj_lock(vdev_cp_stats_priv);
	vdev_mc_stats = vdev_cp_stats_priv->vdev_stats;
	info.vdev_summary_stats[0].vdev_id = last_req->vdev_id;
	info.vdev_summary_stats[0].stats = vdev_mc_stats->vdev_summary_stats;
	info.vdev_chain_rssi[0].vdev_id = last_req->vdev_id;
	qdf_mem_copy(info.vdev_chain_rssi[0].chain_rssi,
		     vdev_mc_stats->chain_rssi,
		     sizeof(vdev_mc_stats->chain_rssi));
	info.tx_rate_flags = vdev_mc_stats->tx_rate_flags;

	info.bcn_protect_stats = vdev_mc_stats->pmf_bcn_stats;
	wlan_cp_stats_vdev_obj_unlock(vdev_cp_stats_priv);

	info.peer_adv_stats = qdf_mem_malloc(sizeof(*info.peer_adv_stats));
	if (!info.peer_adv_stats)
		goto end;

	wlan_cp_stats_peer_obj_lock(peer_cp_stats_priv);
	peer_mc_stats = peer_cp_stats_priv->peer_stats;
	/*
	 * The linkspeed returned by fw is in kbps so convert
	 * it in units of 100kbps which is expected by UMAC
	 */
	info.tx_rate = peer_mc_stats->tx_rate / 100;
	info.rx_rate = peer_mc_stats->rx_rate / 100;

	if (peer_mc_stats->adv_stats) {
		info.num_peer_adv_stats = 1;
		qdf_mem_copy(info.peer_adv_stats,
			     peer_mc_stats->adv_stats,
			     sizeof(*peer_mc_stats->adv_stats));
	}

	wlan_cp_stats_peer_obj_unlock(peer_cp_stats_priv);

end:
	if (info.vdev_summary_stats && info.vdev_chain_rssi)
		get_station_stats_cb(&info, last_req->cookie);

	ucfg_mc_cp_stats_free_stats_resources(&info);

	if (peer)
		wlan_objmgr_peer_release_ref(peer, WLAN_CP_STATS_ID);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_CP_STATS_ID);
}

static void tgt_mc_cp_stats_extract_station_stats(
				struct wlan_objmgr_psoc *psoc,
				struct stats_event *ev)
{
	QDF_STATUS status;
	struct request_info last_req = {0};
	bool pending = false;

	status = ucfg_mc_cp_stats_get_pending_req(psoc,
						  TYPE_STATION_STATS,
						  &last_req);
	if (QDF_IS_STATUS_ERROR(status)) {
		cp_stats_err("ucfg_mc_cp_stats_get_pending_req failed");
		return;
	}

	tgt_mc_cp_stats_extract_tx_power(psoc, ev, true);
	tgt_mc_cp_stats_extract_peer_stats(psoc, ev, true);
	tgt_mc_cp_stats_extract_vdev_summary_stats(psoc, ev);
	tgt_mc_cp_stats_extract_vdev_chain_rssi_stats(psoc, ev);
	tgt_mc_cp_stats_extract_pmf_bcn_stats(psoc, ev);

	/*
	 * PEER stats are the last stats sent for get_station statistics.
	 * reset type_map bit for station stats .
	 */
	if (tgt_mc_cp_stats_is_last_event(ev, TYPE_STATION_STATS)) {
		ucfg_mc_cp_stats_reset_pending_req(psoc, TYPE_STATION_STATS,
						   &last_req,
						   &pending);
		if (pending && last_req.u.get_station_stats_cb)
			tgt_mc_cp_stats_prepare_n_send_raw_station_stats(
							psoc, &last_req);
	}
}

static void tgt_mc_cp_send_lost_link_stats(struct wlan_objmgr_psoc *psoc,
					   struct stats_event *ev)
{
	struct psoc_cp_stats *psoc_cp_stats_priv;

	psoc_cp_stats_priv = wlan_cp_stats_get_psoc_stats_obj(psoc);
	if (psoc_cp_stats_priv && psoc_cp_stats_priv->legacy_stats_cb)
		psoc_cp_stats_priv->legacy_stats_cb(ev);
}

#ifdef WLAN_SUPPORT_INFRA_CTRL_PATH_STATS
QDF_STATUS tgt_mc_cp_stats_process_infra_stats_event(
				struct wlan_objmgr_psoc *psoc,
				struct infra_cp_stats_event *infra_event)
{
	if (!infra_event)
		return QDF_STATUS_E_NULL_VALUE;

	tgt_mc_infra_cp_stats_extract_twt_stats(psoc, infra_event);

	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS tgt_mc_cp_stats_process_stats_event(struct wlan_objmgr_psoc *psoc,
					       struct stats_event *ev)
{
	if (ucfg_mc_cp_stats_is_req_pending(psoc, TYPE_CONNECTION_TX_POWER))
		tgt_mc_cp_stats_extract_tx_power(psoc, ev, false);

	if (ucfg_mc_cp_stats_is_req_pending(psoc, TYPE_PEER_STATS))
		tgt_mc_cp_stats_extract_peer_stats(psoc, ev, false);

	if (ucfg_mc_cp_stats_is_req_pending(psoc, TYPE_STATION_STATS))
		tgt_mc_cp_stats_extract_station_stats(psoc, ev);

	if (ucfg_mc_cp_stats_is_req_pending(psoc, TYPE_MIB_STATS))
		tgt_mc_cp_stats_extract_mib_stats(psoc, ev);

	if (ucfg_mc_cp_stats_is_req_pending(psoc, TYPE_PEER_STATS_INFO_EXT))
		tgt_mc_cp_stats_extract_peer_stats_info_ext(psoc, ev);

	tgt_mc_cp_stats_extract_cca_stats(psoc, ev);

	tgt_mc_cp_send_lost_link_stats(psoc, ev);
	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_BIG_DATA_STATS
QDF_STATUS
tgt_mc_cp_stats_process_big_data_stats_event(struct wlan_objmgr_psoc *psoc,
					     struct big_data_stats_event *ev)
{
	QDF_STATUS status;
	struct request_info last_req = {0};
	bool pending = false;

	if (!ev) {
		cp_stats_err("invalid data");
		return QDF_STATUS_E_INVAL;
	}

	status = ucfg_mc_cp_stats_get_pending_req(psoc,
						  TYPE_BIG_DATA_STATS,
						  &last_req);
	if (QDF_IS_STATUS_ERROR(status)) {
		cp_stats_err("ucfg_mc_cp_stats_get_pending_req failed");
		return QDF_STATUS_E_FAILURE;
	}

	ucfg_mc_cp_stats_reset_pending_req(psoc, TYPE_BIG_DATA_STATS,
					   &last_req, &pending);

	if (last_req.u.get_big_data_stats_cb && pending) {
		last_req.u.get_big_data_stats_cb(ev, last_req.cookie);
		last_req.u.get_big_data_stats_cb = NULL;
	} else {
		cp_stats_err("callback to send big data stats not found");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS tgt_mc_cp_stats_inc_wake_lock_stats(struct wlan_objmgr_psoc *psoc,
					       uint32_t reason,
					       struct wake_lock_stats *stats,
					       uint32_t *unspecified_wake_count)
{
	struct wlan_lmac_if_cp_stats_tx_ops *tx_ops;

	tx_ops = target_if_cp_stats_get_tx_ops(psoc);
	if (!tx_ops)
		return QDF_STATUS_E_NULL_VALUE;

	tx_ops->inc_wake_lock_stats(reason, stats, unspecified_wake_count);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS tgt_send_mc_cp_stats_req(struct wlan_objmgr_psoc *psoc,
				    enum stats_req_type type,
				    struct request_info *req)
{
	struct wlan_lmac_if_cp_stats_tx_ops *tx_ops;
	QDF_STATUS status;

	tx_ops = target_if_cp_stats_get_tx_ops(psoc);
	if (!tx_ops) {
		cp_stats_err("could not get tx_ops");
		return QDF_STATUS_E_NULL_VALUE;
	}

	switch (type) {
	case TYPE_PEER_STATS_INFO_EXT:
		if (!tx_ops->send_req_peer_stats) {
			cp_stats_err("could not get send_req_peer_stats");
			return QDF_STATUS_E_NULL_VALUE;
		}
		status = tx_ops->send_req_peer_stats(psoc, req);
		break;
	case TYPE_BIG_DATA_STATS:
		status = send_big_data_stats_req(tx_ops, psoc, req);
		break;
	default:
		if (!tx_ops->send_req_stats) {
			cp_stats_err("could not get send_req_stats");
			return QDF_STATUS_E_NULL_VALUE;
		}
		status = tx_ops->send_req_stats(psoc, type, req);
	}

	return status;
}
