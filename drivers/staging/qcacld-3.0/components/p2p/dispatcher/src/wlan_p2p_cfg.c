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
 * DOC: This file contains p2p configures interface definitions
 */

#include <wlan_objmgr_psoc_obj.h>
#include "wlan_p2p_public_struct.h"
#include "wlan_p2p_cfg_api.h"
#include "../../core/src/wlan_p2p_main.h"
#include "wlan_mlme_ucfg_api.h"

static inline struct p2p_soc_priv_obj *
wlan_psoc_get_p2p_object(struct wlan_objmgr_psoc *psoc)
{
	return wlan_objmgr_psoc_get_comp_private_obj(psoc,
					WLAN_UMAC_COMP_P2P);
}

QDF_STATUS
cfg_p2p_get_go_keepalive_period(struct wlan_objmgr_psoc *psoc,
				uint32_t *period)
{
	struct p2p_soc_priv_obj *p2p_soc_obj;

	p2p_soc_obj = wlan_psoc_get_p2p_object(psoc);
	if (!p2p_soc_obj) {
		*period = 0;
		p2p_err("p2p psoc null");
		return QDF_STATUS_E_INVAL;
	}

	*period = p2p_soc_obj->param.go_keepalive_period;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
cfg_p2p_get_go_link_monitor_period(struct wlan_objmgr_psoc *psoc,
				   uint32_t *period)
{
	struct p2p_soc_priv_obj *p2p_soc_obj;

	p2p_soc_obj = wlan_psoc_get_p2p_object(psoc);
	if (!p2p_soc_obj) {
		*period = 0;
		p2p_err("p2p psoc null");
		return QDF_STATUS_E_INVAL;
	}

	*period = p2p_soc_obj->param.go_link_monitor_period;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
cfg_p2p_get_device_addr_admin(struct wlan_objmgr_psoc *psoc,
			      bool *enable)
{
	struct p2p_soc_priv_obj *p2p_soc_obj;

	p2p_soc_obj = wlan_psoc_get_p2p_object(psoc);
	if (!p2p_soc_obj) {
		*enable = false;
		p2p_err("p2p psoc null");
		return QDF_STATUS_E_INVAL;
	}

	*enable = p2p_soc_obj->param.p2p_device_addr_admin;

	return QDF_STATUS_SUCCESS;
}

bool cfg_p2p_is_roam_config_disabled(struct wlan_objmgr_psoc *psoc)
{
	uint32_t sta_roam_disable = 0;

	if (ucfg_mlme_get_roam_disable_config(psoc, &sta_roam_disable) ==
	    QDF_STATUS_SUCCESS)
		return sta_roam_disable & LFR3_STA_ROAM_DISABLE_BY_P2P;

	return false;
}
