/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: define internal APIs related to the mlme TWT functionality
 */

#include "cfg_mlme_twt.h"
#include "cfg_ucfg_api.h"
#include "wlan_mlme_main.h"
#include "wlan_psoc_mlme_api.h"
#include "wlan_vdev_mlme_api.h"
#include "wlan_mlme_api.h"
#include "wlan_mlme_twt_api.h"

bool mlme_is_max_twt_sessions_reached(struct wlan_objmgr_psoc *psoc,
				      struct qdf_mac_addr *peer_mac,
				      uint8_t dialog_id)
{
	struct peer_mlme_priv_obj *peer_priv;
	struct wlan_objmgr_peer *peer;
	uint8_t i;
	uint8_t num_twt_sessions = 0, max_twt_sessions;

	peer = wlan_objmgr_get_peer_by_mac(psoc, peer_mac->bytes,
					   WLAN_MLME_NB_ID);
	if (!peer) {
		mlme_legacy_err("Peer object not found");
		return true;
	}

	peer_priv = wlan_objmgr_peer_get_comp_private_obj(peer,
							  WLAN_UMAC_COMP_MLME);
	if (!peer_priv) {
		wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);
		mlme_legacy_err("peer mlme component object is NULL");
		return true;
	}

	max_twt_sessions = peer_priv->twt_ctx.num_twt_sessions;
	for (i = 0; i < max_twt_sessions; i++) {
		uint8_t existing_session_dialog_id =
				peer_priv->twt_ctx.session_info[i].dialog_id;

		if (existing_session_dialog_id != WLAN_ALL_SESSIONS_DIALOG_ID &&
		    existing_session_dialog_id != dialog_id)
			num_twt_sessions++;
	}
	wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);

	mlme_legacy_debug("num_twt_sessions:%d max_twt_sessions:%d",
			  num_twt_sessions, max_twt_sessions);
	return num_twt_sessions == max_twt_sessions;
}

bool mlme_is_twt_setup_in_progress(struct wlan_objmgr_psoc *psoc,
				   struct qdf_mac_addr *peer_mac,
				   uint8_t dialog_id)
{
	struct peer_mlme_priv_obj *peer_priv;
	struct wlan_objmgr_peer *peer;
	uint8_t existing_session_dialog_id;
	uint8_t i;

	peer = wlan_objmgr_get_peer_by_mac(psoc, peer_mac->bytes,
					   WLAN_MLME_NB_ID);
	if (!peer) {
		mlme_legacy_err("Peer object not found");
		return false;
	}

	peer_priv = wlan_objmgr_peer_get_comp_private_obj(peer,
							  WLAN_UMAC_COMP_MLME);
	if (!peer_priv) {
		wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);
		mlme_legacy_err("peer mlme component object is NULL");
		return false;
	}

	for (i = 0; i < peer_priv->twt_ctx.num_twt_sessions; i++) {
		bool setup_done = peer_priv->twt_ctx.session_info[i].setup_done;
		existing_session_dialog_id =
			peer_priv->twt_ctx.session_info[i].dialog_id;
		if (existing_session_dialog_id == dialog_id &&
		    existing_session_dialog_id != WLAN_ALL_SESSIONS_DIALOG_ID &&
		    !setup_done) {
			wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);
			return true;
		}
	}
	wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);

	return false;
}

void mlme_add_twt_session(struct wlan_objmgr_psoc *psoc,
			  struct qdf_mac_addr *peer_mac,
			  uint8_t dialog_id)
{
	struct peer_mlme_priv_obj *peer_priv;
	struct wlan_objmgr_peer *peer;
	uint8_t i;

	peer = wlan_objmgr_get_peer_by_mac(psoc, peer_mac->bytes,
					   WLAN_MLME_NB_ID);
	if (!peer) {
		mlme_legacy_err("Peer object not found");
		return;
	}

	peer_priv = wlan_objmgr_peer_get_comp_private_obj(peer,
							  WLAN_UMAC_COMP_MLME);
	if (!peer_priv) {
		wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);
		mlme_legacy_err("peer mlme component object is NULL");
		return;
	}

	for (i = 0; i < peer_priv->twt_ctx.num_twt_sessions; i++) {
		if (peer_priv->twt_ctx.session_info[i].dialog_id ==
		    WLAN_ALL_SESSIONS_DIALOG_ID) {
			peer_priv->twt_ctx.session_info[i].dialog_id =
							dialog_id;
			break;
		}
	}

	wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);
}

void mlme_set_twt_setup_done(struct wlan_objmgr_psoc *psoc,
			     struct qdf_mac_addr *peer_mac,
			     uint8_t dialog_id, bool is_set)
{
	struct peer_mlme_priv_obj *peer_priv;
	struct wlan_objmgr_peer *peer;
	uint8_t i;

	peer = wlan_objmgr_get_peer_by_mac(psoc, peer_mac->bytes,
					   WLAN_MLME_NB_ID);
	if (!peer) {
		mlme_legacy_err("Peer object not found");
		return;
	}

	peer_priv = wlan_objmgr_peer_get_comp_private_obj(peer,
							  WLAN_UMAC_COMP_MLME);
	if (!peer_priv) {
		wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);
		mlme_legacy_err(" peer mlme component object is NULL");
		return;
	}

	for (i = 0; i < peer_priv->twt_ctx.num_twt_sessions; i++) {
		if (peer_priv->twt_ctx.session_info[i].dialog_id == dialog_id) {
			peer_priv->twt_ctx.session_info[i].setup_done = is_set;
			mlme_legacy_debug("setup done:%d dialog:%d", is_set,
					  dialog_id);
			break;
		}
	}

	wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);
}

QDF_STATUS mlme_init_twt_context(struct wlan_objmgr_psoc *psoc,
				 struct qdf_mac_addr *peer_mac,
				 uint8_t dialog_id)
{
	struct peer_mlme_priv_obj *peer_priv;
	struct wlan_objmgr_peer *peer;
	uint8_t i;

	peer = wlan_objmgr_get_peer_by_mac(psoc, peer_mac->bytes,
					   WLAN_MLME_NB_ID);
	if (!peer) {
		mlme_legacy_err("Peer object not found");
		return QDF_STATUS_E_FAILURE;
	}

	peer_priv = wlan_objmgr_peer_get_comp_private_obj(peer,
							  WLAN_UMAC_COMP_MLME);
	if (!peer_priv) {
		wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);
		mlme_legacy_err("peer mlme component object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	peer_priv->twt_ctx.num_twt_sessions = WLAN_MAX_TWT_SESSIONS_PER_PEER;
	for (i = 0; i < peer_priv->twt_ctx.num_twt_sessions; i++) {
		if (peer_priv->twt_ctx.session_info[i].dialog_id == dialog_id ||
		    dialog_id == WLAN_ALL_SESSIONS_DIALOG_ID) {
			peer_priv->twt_ctx.session_info[i].setup_done = false;
			peer_priv->twt_ctx.session_info[i].dialog_id =
					WLAN_ALL_SESSIONS_DIALOG_ID;
			mlme_set_twt_command_in_progress(
				psoc, peer_mac,
				peer_priv->twt_ctx.session_info[i].dialog_id,
				WLAN_TWT_NONE);
		}
	}

	mlme_legacy_debug("init done");

	wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
mlme_init_all_peers_twt_context(struct wlan_objmgr_psoc *psoc,
				uint8_t vdev_id,
				uint8_t dialog_id)
{
	qdf_list_t *peer_list;
	struct wlan_objmgr_peer *peer, *peer_next;
	struct wlan_objmgr_vdev *vdev;
	struct peer_mlme_priv_obj *peer_priv;

	if (!psoc) {
		mlme_legacy_err("psoc is NULL, dialog_id: %d", dialog_id);
		return QDF_STATUS_E_NULL_VALUE;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_legacy_err("vdev is NULL, vdev_id: %d dialog_id: %d",
				vdev_id, dialog_id);
		return QDF_STATUS_E_NULL_VALUE;
	}

	peer_list = &vdev->vdev_objmgr.wlan_peer_list;
	if (!peer_list) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		mlme_legacy_err("Peer list for vdev obj is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

        peer = wlan_vdev_peer_list_peek_active_head(vdev, peer_list,
                                                    WLAN_MLME_NB_ID);
	while (peer) {
		peer_priv = wlan_objmgr_peer_get_comp_private_obj(peer,
							  WLAN_UMAC_COMP_MLME);
		if (peer_priv) {
			uint8_t i = 0;
			uint8_t num_twt_sessions =
					WLAN_MAX_TWT_SESSIONS_PER_PEER;

			peer_priv->twt_ctx.num_twt_sessions =
					num_twt_sessions;
			for (i = 0; i < num_twt_sessions; i++) {
				uint8_t existing_dialog_id =
				peer_priv->twt_ctx.session_info[i].dialog_id;

				if (existing_dialog_id == dialog_id ||
				    dialog_id == WLAN_ALL_SESSIONS_DIALOG_ID) {
					peer_priv->twt_ctx.session_info[i].setup_done = false;
					peer_priv->twt_ctx.session_info[i].dialog_id =
							WLAN_ALL_SESSIONS_DIALOG_ID;
				}
			}
		}

		peer_next =
			wlan_peer_get_next_active_peer_of_vdev(vdev,
							       peer_list,
							       peer,
							       WLAN_MLME_NB_ID);

		wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);
		peer = peer_next;
        }

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
	mlme_legacy_debug("init done");
        return QDF_STATUS_SUCCESS;
}


bool mlme_is_twt_setup_done(struct wlan_objmgr_psoc *psoc,
			    struct qdf_mac_addr *peer_mac, uint8_t dialog_id)
{
	struct peer_mlme_priv_obj *peer_priv;
	struct wlan_objmgr_peer *peer;
	bool is_setup_done = false;
	uint8_t i;

	peer = wlan_objmgr_get_peer_by_mac(psoc, peer_mac->bytes,
					   WLAN_MLME_NB_ID);
	if (!peer) {
		mlme_legacy_err("Peer object not found");
		return false;
	}

	peer_priv = wlan_objmgr_peer_get_comp_private_obj(peer,
							  WLAN_UMAC_COMP_MLME);
	if (!peer_priv) {
		wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);
		mlme_legacy_err("peer mlme component object is NULL");
		return false;
	}

	for (i = 0; i < peer_priv->twt_ctx.num_twt_sessions; i++) {
		if (peer_priv->twt_ctx.session_info[i].dialog_id == dialog_id ||
		    dialog_id == WLAN_ALL_SESSIONS_DIALOG_ID) {
			is_setup_done =
				peer_priv->twt_ctx.session_info[i].setup_done;

			if (dialog_id != WLAN_ALL_SESSIONS_DIALOG_ID ||
			    is_setup_done)
				break;
		}
	}

	wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);

	return is_setup_done;
}

void mlme_twt_set_wait_for_notify(struct wlan_objmgr_psoc *psoc,
				  uint32_t vdev_id, bool is_set)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_legacy_err("vdev object not found");
		return;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		mlme_legacy_err("vdev legacy private object is NULL");
		return;
	}

	mlme_priv->twt_wait_for_notify = is_set;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
}

void mlme_set_twt_session_state(struct wlan_objmgr_psoc *psoc,
				struct qdf_mac_addr *peer_mac,
				uint8_t dialog_id,
				enum wlan_twt_session_state state)
{
	struct wlan_objmgr_peer *peer;
	struct peer_mlme_priv_obj *peer_priv;
	uint8_t i;

	peer = wlan_objmgr_get_peer_by_mac(psoc, peer_mac->bytes,
					   WLAN_MLME_NB_ID);
	if (!peer) {
		mlme_legacy_err("Peer object not found");
		return;
	}

	peer_priv = wlan_objmgr_peer_get_comp_private_obj(peer,
							  WLAN_UMAC_COMP_MLME);
	if (!peer_priv) {
		wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);
		mlme_err(" peer mlme component object is NULL");
		return;
	}

	mlme_debug("set_state:%d for dialog_id:%d", state, dialog_id);
	for (i = 0; i < peer_priv->twt_ctx.num_twt_sessions; i++) {
		if (peer_priv->twt_ctx.session_info[i].dialog_id == dialog_id ||
		    dialog_id == WLAN_ALL_SESSIONS_DIALOG_ID) {
			peer_priv->twt_ctx.session_info[i].state = state;
			break;
		}
	}

	wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);
}

enum wlan_twt_session_state
mlme_get_twt_session_state(struct wlan_objmgr_psoc *psoc,
			   struct qdf_mac_addr *peer_mac, uint8_t dialog_id)
{
	struct wlan_objmgr_peer *peer;
	struct peer_mlme_priv_obj *peer_priv;
	uint8_t i;

	peer = wlan_objmgr_get_peer_by_mac(psoc, peer_mac->bytes,
					   WLAN_MLME_NB_ID);
	if (!peer) {
		mlme_legacy_err("Peer object not found");
		return WLAN_TWT_SETUP_STATE_NOT_ESTABLISHED;
	}

	peer_priv = wlan_objmgr_peer_get_comp_private_obj(peer,
							  WLAN_UMAC_COMP_MLME);
	if (!peer_priv) {
		wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);
		mlme_legacy_err(" peer mlme object is NULL");
		return WLAN_TWT_SETUP_STATE_NOT_ESTABLISHED;
	}

	for (i = 0; i < peer_priv->twt_ctx.num_twt_sessions; i++) {
		if (peer_priv->twt_ctx.session_info[i].dialog_id == dialog_id &&
		    dialog_id != WLAN_ALL_SESSIONS_DIALOG_ID) {
			wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);
			return peer_priv->twt_ctx.session_info[i].state;
		}
	}

	wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);

	return WLAN_TWT_SETUP_STATE_NOT_ESTABLISHED;
}

uint8_t mlme_get_twt_peer_capabilities(struct wlan_objmgr_psoc *psoc,
				       struct qdf_mac_addr *peer_mac)
{
	struct peer_mlme_priv_obj *peer_priv;
	struct wlan_objmgr_peer *peer;

	peer = wlan_objmgr_get_peer_by_mac(psoc, peer_mac->bytes,
					   WLAN_MLME_NB_ID);
	if (!peer) {
		mlme_legacy_err("Peer object not found");
		return false;
	}

	peer_priv = wlan_objmgr_peer_get_comp_private_obj(peer,
							  WLAN_UMAC_COMP_MLME);
	wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);
	if (!peer_priv) {
		mlme_legacy_err("peer mlme object is NULL");
		return 0;
	}

	return peer_priv->twt_ctx.peer_capability;
}

void mlme_set_twt_peer_capabilities(struct wlan_objmgr_psoc *psoc,
				    struct qdf_mac_addr *peer_mac,
				    tDot11fIEhe_cap *he_cap,
				    tDot11fIEhe_op *he_op)
{
	struct wlan_objmgr_peer *peer;
	struct peer_mlme_priv_obj *peer_priv;
	uint8_t caps = 0;

	peer = wlan_objmgr_get_peer_by_mac(psoc, peer_mac->bytes,
					   WLAN_MLME_NB_ID);
	if (!peer) {
		mlme_legacy_err("Peer object not found");
		return;
	}

	peer_priv = wlan_objmgr_peer_get_comp_private_obj(peer,
							  WLAN_UMAC_COMP_MLME);
	if (!peer_priv) {
		wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);
		mlme_legacy_err("peer mlme object is NULL");
		return;
	}

	if (he_cap->twt_request)
		caps |= WLAN_TWT_CAPA_REQUESTOR;

	if (he_cap->twt_responder)
		caps |= WLAN_TWT_CAPA_RESPONDER;

	if (he_cap->broadcast_twt)
		caps |= WLAN_TWT_CAPA_BROADCAST;

	if (he_cap->flex_twt_sched)
		caps |= WLAN_TWT_CAPA_FLEXIBLE;

	if (he_op->twt_required)
		caps |= WLAN_TWT_CAPA_REQUIRED;

	peer_priv->twt_ctx.peer_capability = caps;
	wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);
}

bool mlme_is_twt_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return cfg_default(CFG_ENABLE_TWT);

	return mlme_obj->cfg.twt_cfg.is_twt_enabled;
}

#ifdef WLAN_FEATURE_11AX
bool mlme_is_flexible_twt_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return false;

	return mlme_obj->cfg.he_caps.dot11_he_cap.flex_twt_sched;
}
#endif

QDF_STATUS
mlme_sap_set_twt_all_peers_cmd_in_progress(struct wlan_objmgr_psoc *psoc,
					   uint8_t vdev_id,
					   uint8_t dialog_id,
					   enum wlan_twt_commands cmd)
{
	qdf_list_t *peer_list;
	struct wlan_objmgr_peer *peer, *peer_next;
	struct wlan_objmgr_vdev *vdev;
	struct peer_mlme_priv_obj *peer_priv;

	if (!psoc) {
		mlme_legacy_err("psoc is NULL, dialog_id: %d", dialog_id);
		return QDF_STATUS_E_NULL_VALUE;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_legacy_err("vdev is NULL, vdev_id: %d dialog_id: %d",
				vdev_id, dialog_id);
		return QDF_STATUS_E_NULL_VALUE;
	}

	peer_list = &vdev->vdev_objmgr.wlan_peer_list;
	if (!peer_list) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		mlme_legacy_err("Peer list for vdev obj is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

        peer = wlan_vdev_peer_list_peek_active_head(vdev, peer_list,
                                                    WLAN_MLME_NB_ID);
	while (peer) {
		peer_priv = wlan_objmgr_peer_get_comp_private_obj(peer,
							  WLAN_UMAC_COMP_MLME);
		if (peer_priv) {
			uint8_t i = 0;
			uint8_t num_twt_sessions =
				peer_priv->twt_ctx.num_twt_sessions;

			for (i = 0; i < num_twt_sessions; i++) {
				uint8_t existing_dialog_id =
				peer_priv->twt_ctx.session_info[i].dialog_id;

				if (existing_dialog_id == dialog_id ||
				    dialog_id == WLAN_ALL_SESSIONS_DIALOG_ID) {
					peer_priv->twt_ctx.session_info[i].active_cmd = cmd;

					if (dialog_id !=
					    WLAN_ALL_SESSIONS_DIALOG_ID) {
						break;
					}
				}
			}
		}

		peer_next =
			wlan_peer_get_next_active_peer_of_vdev(vdev,
							       peer_list,
							       peer,
							       WLAN_MLME_NB_ID);

		wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);
		peer = peer_next;
        }

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
        return QDF_STATUS_SUCCESS;
}

bool
mlme_twt_any_peer_cmd_in_progress(struct wlan_objmgr_psoc *psoc,
				  uint8_t vdev_id,
				  uint8_t dialog_id,
				  enum wlan_twt_commands cmd)
{
	qdf_list_t *peer_list;
	struct wlan_objmgr_peer *peer, *peer_next;
	struct wlan_objmgr_vdev *vdev;
	struct peer_mlme_priv_obj *peer_priv;
	bool cmd_in_progress = false;

	if (!psoc) {
		mlme_legacy_err("psoc is NULL, dialog_id: %d", dialog_id);
		return false;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_legacy_err("vdev is NULL, vdev_id: %d dialog_id: %d",
				vdev_id, dialog_id);
		return false;
	}

	peer_list = &vdev->vdev_objmgr.wlan_peer_list;
	if (!peer_list) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		mlme_legacy_err("Peer list for vdev obj is NULL");
		return false;
	}

        peer = wlan_vdev_peer_list_peek_active_head(vdev, peer_list,
                                                    WLAN_MLME_NB_ID);
	while (peer) {
		peer_priv = wlan_objmgr_peer_get_comp_private_obj(peer,
							  WLAN_UMAC_COMP_MLME);
		if (peer_priv) {
			uint8_t i = 0;
			uint8_t num_twt_sessions =
				peer_priv->twt_ctx.num_twt_sessions;

			for (i = 0; i < num_twt_sessions; i++) {
				enum wlan_twt_commands active_cmd;
				uint8_t existing_dialog_id;

				active_cmd =
				 peer_priv->twt_ctx.session_info[i].active_cmd;
				existing_dialog_id =
				 peer_priv->twt_ctx.session_info[i].dialog_id;

				if (existing_dialog_id == dialog_id ||
				    dialog_id == WLAN_ALL_SESSIONS_DIALOG_ID) {
					cmd_in_progress = (active_cmd == cmd);

					if (dialog_id !=
						WLAN_ALL_SESSIONS_DIALOG_ID ||
					    cmd_in_progress) {
						wlan_objmgr_peer_release_ref(
							peer,
							WLAN_MLME_NB_ID);
						wlan_objmgr_vdev_release_ref(
							vdev,
							WLAN_MLME_NB_ID);
						return cmd_in_progress;
					}
				}
			}
		}

		peer_next =
			wlan_peer_get_next_active_peer_of_vdev(vdev,
							       peer_list,
							       peer,
							       WLAN_MLME_NB_ID);

		wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);
		peer = peer_next;
        }

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
	return cmd_in_progress;
}

bool mlme_sap_twt_peer_is_cmd_in_progress(struct wlan_objmgr_psoc *psoc,
				     struct qdf_mac_addr *peer_mac,
				     uint8_t dialog_id,
				     enum wlan_twt_commands cmd)
{
	struct wlan_objmgr_peer *peer;
	struct peer_mlme_priv_obj *peer_priv;
	uint8_t i = 0;
	bool cmd_in_progress = false;

	peer = wlan_objmgr_get_peer_by_mac(psoc, peer_mac->bytes,
					   WLAN_MLME_NB_ID);
	if (!peer) {
		mlme_legacy_err("Peer object not found");
		return false;
	}

	peer_priv = wlan_objmgr_peer_get_comp_private_obj(peer,
							  WLAN_UMAC_COMP_MLME);
	if (!peer_priv) {
		wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);
		mlme_legacy_err(" peer mlme component object is NULL");
		return false;
	}

	for (i = 0; i < peer_priv->twt_ctx.num_twt_sessions; i++) {
		enum wlan_twt_commands active_cmd;
		uint8_t existing_dialog_id;

		active_cmd =
			peer_priv->twt_ctx.session_info[i].active_cmd;
		existing_dialog_id =
			peer_priv->twt_ctx.session_info[i].dialog_id;

		if (existing_dialog_id == dialog_id ||
		    dialog_id == WLAN_ALL_SESSIONS_DIALOG_ID ||
		    existing_dialog_id == WLAN_ALL_SESSIONS_DIALOG_ID) {
			cmd_in_progress = (active_cmd == cmd);

			if (dialog_id != WLAN_ALL_SESSIONS_DIALOG_ID ||
			    cmd_in_progress) {
				break;
			}
		}
	}

	wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);
	return cmd_in_progress;
}

QDF_STATUS mlme_set_twt_command_in_progress(struct wlan_objmgr_psoc *psoc,
					    struct qdf_mac_addr *peer_mac,
					    uint8_t dialog_id,
					    enum wlan_twt_commands cmd)
{
	struct wlan_objmgr_peer *peer;
	struct peer_mlme_priv_obj *peer_priv;
	uint8_t i = 0;

	peer = wlan_objmgr_get_peer_by_mac(psoc, peer_mac->bytes,
					   WLAN_MLME_NB_ID);
	if (!peer) {
		mlme_legacy_err("Peer object not found");
		return QDF_STATUS_E_FAILURE;
	}

	peer_priv = wlan_objmgr_peer_get_comp_private_obj(peer,
							  WLAN_UMAC_COMP_MLME);
	if (!peer_priv) {
		wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);
		mlme_legacy_err(" peer mlme component object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	for (i = 0; i < peer_priv->twt_ctx.num_twt_sessions; i++) {
		if (peer_priv->twt_ctx.session_info[i].dialog_id == dialog_id ||
		    dialog_id == WLAN_ALL_SESSIONS_DIALOG_ID) {
			peer_priv->twt_ctx.session_info[i].active_cmd = cmd;
			if (dialog_id != WLAN_ALL_SESSIONS_DIALOG_ID)
				break;
		}
	}

	wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);

	return QDF_STATUS_SUCCESS;
}

bool mlme_is_twt_notify_in_progress(struct wlan_objmgr_psoc *psoc,
				    uint32_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;
	bool is_twt_notify_in_progress;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);

	if (!vdev) {
		mlme_legacy_err("vdev object not found");
		return false;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		mlme_legacy_err("vdev legacy private object is NULL");
		return false;
	}

	is_twt_notify_in_progress = mlme_priv->twt_wait_for_notify;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return is_twt_notify_in_progress;
}

bool mlme_twt_is_command_in_progress(struct wlan_objmgr_psoc *psoc,
				     struct qdf_mac_addr *peer_mac,
				     uint8_t dialog_id,
				     enum wlan_twt_commands cmd,
				     enum wlan_twt_commands *pactive_cmd)
{
	struct wlan_objmgr_peer *peer;
	struct peer_mlme_priv_obj *peer_priv;
	enum wlan_twt_commands active_cmd;
	uint8_t i = 0;
	bool is_command_in_progress = false;

	peer = wlan_objmgr_get_peer_by_mac(psoc, peer_mac->bytes,
					   WLAN_MLME_NB_ID);
	if (!peer) {
		mlme_legacy_err("Peer object not found");
		return false;
	}

	peer_priv = wlan_objmgr_peer_get_comp_private_obj(peer,
							  WLAN_UMAC_COMP_MLME);
	if (!peer_priv) {
		wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);
		mlme_legacy_err(" peer mlme component object is NULL");
		return false;
	}

	for (i = 0; i < peer_priv->twt_ctx.num_twt_sessions; i++) {
		active_cmd = peer_priv->twt_ctx.session_info[i].active_cmd;

		if (pactive_cmd)
			*pactive_cmd = active_cmd;

		if (peer_priv->twt_ctx.session_info[i].dialog_id == dialog_id ||
		    dialog_id == WLAN_ALL_SESSIONS_DIALOG_ID) {
			if (cmd == WLAN_TWT_ANY) {
				is_command_in_progress =
					(active_cmd != WLAN_TWT_NONE);

				if (dialog_id != WLAN_ALL_SESSIONS_DIALOG_ID ||
				    is_command_in_progress)
					break;
			} else {
				is_command_in_progress = (active_cmd == cmd);

				if (dialog_id != WLAN_ALL_SESSIONS_DIALOG_ID ||
				    is_command_in_progress)
					break;
			}
		}
	}

	wlan_objmgr_peer_release_ref(peer, WLAN_MLME_NB_ID);

	return is_command_in_progress;
}

bool mlme_is_24ghz_twt_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return cfg_default(CFG_ENABLE_TWT_24GHZ);

	return mlme_obj->cfg.twt_cfg.enable_twt_24ghz;
}
