/*
 * Copyright (c) 2018-2020 The Linux Foundation. All rights reserved.
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
 * DOC: contains NAN INI configurations
 */

#include "wlan_objmgr_psoc_obj.h"
#include "cfg_nan_api.h"
#include "../../core/src/nan_main_i.h"
#include "wlan_mlme_ucfg_api.h"
#include "cfg_ucfg_api.h"
#include "cfg_nan.h"

static inline struct nan_psoc_priv_obj
		 *cfg_nan_get_priv_obj(struct wlan_objmgr_psoc *psoc)
{
	if (!psoc) {
		nan_err("PSOC obj null");
		return NULL;
	}
	return wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_NAN);
}

bool cfg_nan_get_enable(struct wlan_objmgr_psoc *psoc)
{
	struct nan_psoc_priv_obj *nan_obj = cfg_nan_get_priv_obj(psoc);

	if (!nan_obj) {
		nan_err("NAN obj null");
		return false;
	}
	return nan_obj->cfg_param.enable;
}

bool cfg_nan_get_datapath_enable(struct wlan_objmgr_psoc *psoc)
{
	struct nan_psoc_priv_obj *nan_obj = cfg_nan_get_priv_obj(psoc);

	if (!nan_obj) {
		nan_err("NAN obj null");
		return false;
	}
	return nan_obj->cfg_param.dp_enable;
}

bool cfg_nan_get_ndi_mac_randomize(struct wlan_objmgr_psoc *psoc)
{
	struct nan_psoc_priv_obj *nan_obj = cfg_nan_get_priv_obj(psoc);

	if (!nan_obj) {
		nan_err("NAN obj null");
		return false;
	}
	return nan_obj->cfg_param.ndi_mac_randomize;
}

QDF_STATUS cfg_nan_get_ndp_inactivity_timeout(struct wlan_objmgr_psoc *psoc,
					      uint16_t *val)
{
	struct nan_psoc_priv_obj *nan_obj = cfg_nan_get_priv_obj(psoc);

	if (!nan_obj) {
		nan_err("NAN obj null");
		return QDF_STATUS_E_INVAL;
	}

	*val = nan_obj->cfg_param.ndp_inactivity_timeout;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS cfg_nan_get_ndp_keepalive_period(struct wlan_objmgr_psoc *psoc,
					    uint16_t *val)
{
	struct nan_psoc_priv_obj *nan_obj = cfg_nan_get_priv_obj(psoc);

	if (!nan_obj) {
		nan_err("NAN obj null");
		return QDF_STATUS_E_INVAL;
	}

	*val = nan_obj->cfg_param.ndp_keep_alive_period;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS cfg_nan_get_ndp_max_sessions(struct wlan_objmgr_psoc *psoc,
					uint32_t *val)
{
	struct nan_psoc_priv_obj *nan_obj = cfg_nan_get_priv_obj(psoc);

	if (!nan_obj) {
		nan_err("NAN obj null");
		*val = cfg_default(CFG_NDP_MAX_SESSIONS);
		return QDF_STATUS_E_INVAL;
	}

	*val = nan_obj->cfg_param.max_ndp_sessions;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS cfg_nan_get_max_ndi(struct wlan_objmgr_psoc *psoc, uint32_t *val)
{
	struct nan_psoc_priv_obj *nan_obj = cfg_nan_get_priv_obj(psoc);

	if (!nan_obj) {
		nan_err("NAN obj null");
		*val = cfg_default(CFG_NDI_MAX_SUPPORT);
		return QDF_STATUS_E_INVAL;
	}

	*val = nan_obj->cfg_param.max_ndi;
	return QDF_STATUS_SUCCESS;
}

bool cfg_nan_get_support_mp0_discovery(struct wlan_objmgr_psoc *psoc)
{
	struct nan_psoc_priv_obj *nan_obj = cfg_nan_get_priv_obj(psoc);

	if (!nan_obj) {
		nan_err("NAN obj null");
		return false;
	}

	return nan_obj->cfg_param.support_mp0_discovery;
}

bool cfg_nan_is_roam_config_disabled(struct wlan_objmgr_psoc *psoc)
{
	uint32_t sta_roam_disable;

	if (ucfg_mlme_get_roam_disable_config(psoc, &sta_roam_disable) ==
	    QDF_STATUS_SUCCESS)
		return sta_roam_disable & LFR3_STA_ROAM_DISABLE_BY_NAN;

	return false;
}
