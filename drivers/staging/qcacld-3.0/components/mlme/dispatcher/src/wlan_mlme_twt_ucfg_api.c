/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: define UCFG APIs exposed for TWT by the mlme component
 */

#include "wlan_mlme_main.h"
#include "wlan_mlme_api.h"
#include "wlan_mlme_ucfg_api.h"
#include "cfg_mlme_twt.h"
#include "wlan_mlme_twt_ucfg_api.h"

#if defined(WLAN_SUPPORT_TWT) && defined(WLAN_FEATURE_11AX)
QDF_STATUS
ucfg_mlme_get_twt_requestor(struct wlan_objmgr_psoc *psoc,
			    bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_TWT_REQUESTOR);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.he_caps.dot11_he_cap.twt_request;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_mlme_set_twt_requestor(struct wlan_objmgr_psoc *psoc,
			    bool val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	mlme_obj->cfg.he_caps.dot11_he_cap.twt_request = val;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_mlme_get_twt_responder(struct wlan_objmgr_psoc *psoc,
			    bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_TWT_RESPONDER);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.he_caps.dot11_he_cap.twt_responder;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_mlme_set_twt_responder(struct wlan_objmgr_psoc *psoc,
			    bool val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	mlme_obj->cfg.he_caps.dot11_he_cap.twt_responder = val;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_mlme_set_twt_requestor_flag(struct wlan_objmgr_psoc *psoc, bool val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	mlme_obj->cfg.twt_cfg.req_flag = val;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_mlme_set_twt_responder_flag(struct wlan_objmgr_psoc *psoc, bool val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	mlme_obj->cfg.twt_cfg.res_flag = val;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_mlme_reset_twt_active_cmd(struct wlan_objmgr_psoc *psoc,
			       struct qdf_mac_addr *peer_macaddr,
			       uint8_t dialog_id)
{
	mlme_set_twt_command_in_progress(psoc, peer_macaddr, dialog_id,
					 WLAN_TWT_NONE);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_mlme_get_twt_congestion_timeout(struct wlan_objmgr_psoc *psoc,
				     uint32_t *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		*val = cfg_default(CFG_TWT_CONGESTION_TIMEOUT);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.twt_cfg.twt_congestion_timeout;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_mlme_set_twt_congestion_timeout(struct wlan_objmgr_psoc *psoc,
				     uint32_t val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	mlme_obj->cfg.twt_cfg.twt_congestion_timeout = val;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_mlme_set_enable_twt(struct wlan_objmgr_psoc *psoc,
			 bool val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	mlme_obj->cfg.twt_cfg.is_twt_enabled = val;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_mlme_get_twt_bcast_requestor(struct wlan_objmgr_psoc *psoc,
				  bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		uint32_t b_req_res;

		b_req_res = cfg_default(CFG_BCAST_TWT_REQ_RESP);
		*val = CFG_TWT_GET_BCAST_REQ(b_req_res);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.twt_cfg.is_bcast_requestor_enabled;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_mlme_get_twt_bcast_responder(struct wlan_objmgr_psoc *psoc,
				  bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		uint32_t b_req_res;

		b_req_res = cfg_default(CFG_BCAST_TWT_REQ_RESP);
		*val = CFG_TWT_GET_BCAST_RES(b_req_res);
		return QDF_STATUS_E_INVAL;
	}

	*val = mlme_obj->cfg.twt_cfg.is_bcast_responder_enabled;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_mlme_set_twt_bcast_requestor(struct wlan_objmgr_psoc *psoc,
				  bool val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	mlme_obj->cfg.twt_cfg.is_bcast_requestor_enabled = val;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_mlme_set_twt_bcast_responder(struct wlan_objmgr_psoc *psoc,
				  bool val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	mlme_obj->cfg.twt_cfg.is_bcast_responder_enabled = val;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_mlme_set_twt_nudge_tgt_cap(struct wlan_objmgr_psoc *psoc, bool val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	mlme_obj->cfg.twt_cfg.is_twt_nudge_tgt_cap_enabled = val;

	return QDF_STATUS_SUCCESS;
}

bool ucfg_mlme_get_twt_peer_responder_capabilities(
					struct wlan_objmgr_psoc *psoc,
					struct qdf_mac_addr *peer_mac)
{
	uint8_t peer_cap;

	peer_cap = mlme_get_twt_peer_capabilities(psoc, peer_mac);

	if (peer_cap & WLAN_TWT_CAPA_RESPONDER)
		return true;

	return false;
}

QDF_STATUS
ucfg_mlme_get_twt_nudge_tgt_cap(struct wlan_objmgr_psoc *psoc, bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	*val = mlme_obj->cfg.twt_cfg.is_twt_nudge_tgt_cap_enabled;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_mlme_set_twt_all_twt_tgt_cap(struct wlan_objmgr_psoc *psoc, bool val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	mlme_obj->cfg.twt_cfg.is_all_twt_tgt_cap_enabled = val;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_mlme_get_twt_all_twt_tgt_cap(struct wlan_objmgr_psoc *psoc, bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	*val = mlme_obj->cfg.twt_cfg.is_all_twt_tgt_cap_enabled;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_mlme_set_twt_statistics_tgt_cap(struct wlan_objmgr_psoc *psoc, bool val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	mlme_obj->cfg.twt_cfg.is_twt_statistics_tgt_cap_enabled = val;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_mlme_get_twt_statistics_tgt_cap(struct wlan_objmgr_psoc *psoc, bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	*val = mlme_obj->cfg.twt_cfg.is_twt_statistics_tgt_cap_enabled;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_mlme_set_twt_res_service_cap(struct wlan_objmgr_psoc *psoc, bool val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	mlme_obj->cfg.twt_cfg.twt_res_svc_cap = val;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_mlme_get_twt_res_service_cap(struct wlan_objmgr_psoc *psoc, bool *val)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	*val = mlme_obj->cfg.twt_cfg.twt_res_svc_cap;

	return QDF_STATUS_SUCCESS;
}

#endif
