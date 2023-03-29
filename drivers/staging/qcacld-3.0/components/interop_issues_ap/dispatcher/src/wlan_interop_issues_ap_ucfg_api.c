/*
 * Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
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
 * DOC: This file contains interop issues ap north bound interface definitions
 */
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_interop_issues_ap_ucfg_api.h>
#include <wlan_interop_issues_ap_tgt_api.h>
#include <wlan_cfg80211_interop_issues_ap.h>
#include <wlan_interop_issues_ap_api.h>

QDF_STATUS
ucfg_set_interop_issues_ap_config(struct wlan_objmgr_psoc *psoc,
				  struct wlan_interop_issues_ap_info *rap)
{
	return tgt_set_interop_issues_ap_req(psoc, rap);
}

void ucfg_register_interop_issues_ap_callback(struct wlan_objmgr_pdev *pdev,
				   struct wlan_interop_issues_ap_callbacks *cb)
{
	struct wlan_objmgr_psoc *psoc;
	struct interop_issues_ap_psoc_priv_obj *obj;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		interop_issues_ap_err("psoc object is NULL");
		return;
	}

	obj = interop_issues_ap_get_psoc_priv_obj(psoc);
	if (!obj) {
		interop_issues_ap_err("interop issues ap priv obj is NULL");
		return;
	}

	obj->cbs.os_if_interop_issues_ap_event_handler =
			cb->os_if_interop_issues_ap_event_handler;
}

QDF_STATUS ucfg_interop_issues_ap_psoc_enable(struct wlan_objmgr_psoc *psoc)
{
	return wlan_interop_issues_ap_psoc_enable(psoc);
}

QDF_STATUS ucfg_interop_issues_ap_psoc_disable(struct wlan_objmgr_psoc *psoc)
{
	return wlan_interop_issues_ap_psoc_disable(psoc);
}

QDF_STATUS ucfg_interop_issues_ap_init(void)
{
	return wlan_interop_issues_ap_init();
}

QDF_STATUS ucfg_interop_issues_ap_deinit(void)
{
	return wlan_interop_issues_ap_deinit();
}
