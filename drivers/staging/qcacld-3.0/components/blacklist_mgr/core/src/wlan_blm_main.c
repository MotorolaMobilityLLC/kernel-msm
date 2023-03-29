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
 * DOC: wlan_blm_main.c
 *
 * WLAN Blacklist Mgr related APIs
 *
 */

/* Include files */

#include "target_if_blm.h"
#include <wlan_blm_ucfg_api.h>
#include "cfg_ucfg_api.h"
#include <wlan_blm_core.h>

struct blm_pdev_priv_obj *
blm_get_pdev_obj(struct wlan_objmgr_pdev *pdev)
{
	struct blm_pdev_priv_obj *blm_pdev_obj;

	blm_pdev_obj = wlan_objmgr_pdev_get_comp_private_obj(pdev,
						  WLAN_UMAC_COMP_BLACKLIST_MGR);

	return blm_pdev_obj;
}

struct blm_psoc_priv_obj *
blm_get_psoc_obj(struct wlan_objmgr_psoc *psoc)
{
	struct blm_psoc_priv_obj *blm_psoc_obj;

	blm_psoc_obj = wlan_objmgr_psoc_get_comp_private_obj(psoc,
						  WLAN_UMAC_COMP_BLACKLIST_MGR);

	return blm_psoc_obj;
}

QDF_STATUS
blm_pdev_object_created_notification(struct wlan_objmgr_pdev *pdev,
				     void *arg)
{
	struct blm_pdev_priv_obj *blm_ctx;
	QDF_STATUS status;

	blm_ctx = qdf_mem_malloc(sizeof(*blm_ctx));

	if (!blm_ctx)
		return QDF_STATUS_E_FAILURE;

	status = qdf_mutex_create(&blm_ctx->reject_ap_list_lock);

	if (QDF_IS_STATUS_ERROR(status)) {
		blm_err("Failed to create mutex");
		qdf_mem_free(blm_ctx);
		return status;
	}
	qdf_list_create(&blm_ctx->reject_ap_list, MAX_BAD_AP_LIST_SIZE);

	target_if_blm_register_tx_ops(&blm_ctx->blm_tx_ops);
	status = wlan_objmgr_pdev_component_obj_attach(pdev,
						   WLAN_UMAC_COMP_BLACKLIST_MGR,
						   blm_ctx,
						   QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		blm_err("Failed to attach pdev_ctx with pdev");
		qdf_list_destroy(&blm_ctx->reject_ap_list);
		qdf_mutex_destroy(&blm_ctx->reject_ap_list_lock);
		qdf_mem_free(blm_ctx);
	}

	return status;
}

QDF_STATUS
blm_pdev_object_destroyed_notification(struct wlan_objmgr_pdev *pdev,
				       void *arg)
{
	struct blm_pdev_priv_obj *blm_ctx;

	blm_ctx = blm_get_pdev_obj(pdev);

	if (!blm_ctx) {
		blm_err("BLM Pdev obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	/* Clear away the memory allocated for the bad BSSIDs */
	blm_flush_reject_ap_list(blm_ctx);
	qdf_list_destroy(&blm_ctx->reject_ap_list);
	qdf_mutex_destroy(&blm_ctx->reject_ap_list_lock);

	wlan_objmgr_pdev_component_obj_detach(pdev,
					      WLAN_UMAC_COMP_BLACKLIST_MGR,
					      blm_ctx);
	qdf_mem_free(blm_ctx);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
blm_psoc_object_created_notification(struct wlan_objmgr_psoc *psoc,
				     void *arg)
{
	struct blm_psoc_priv_obj *blm_psoc_obj;
	QDF_STATUS status;

	blm_psoc_obj = qdf_mem_malloc(sizeof(*blm_psoc_obj));

	if (!blm_psoc_obj)
		return QDF_STATUS_E_FAILURE;

	status = wlan_objmgr_psoc_component_obj_attach(psoc,
						   WLAN_UMAC_COMP_BLACKLIST_MGR,
						   blm_psoc_obj,
						   QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		blm_err("Failed to attach psoc_ctx with psoc");
		qdf_mem_free(blm_psoc_obj);
	}

	return status;
}

QDF_STATUS
blm_psoc_object_destroyed_notification(struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct blm_psoc_priv_obj *blm_psoc_obj;

	blm_psoc_obj = blm_get_psoc_obj(psoc);

	if (!blm_psoc_obj) {
		blm_err("BLM psoc obj NULL");
		return QDF_STATUS_E_FAILURE;
	}
	wlan_objmgr_psoc_component_obj_detach(psoc,
					      WLAN_UMAC_COMP_BLACKLIST_MGR,
					      blm_psoc_obj);
	qdf_mem_free(blm_psoc_obj);

	return QDF_STATUS_SUCCESS;
}

static void
blm_init_cfg(struct wlan_objmgr_psoc *psoc, struct blm_config *blm_cfg)
{
	blm_cfg->avoid_list_exipry_time =
				cfg_get(psoc, CFG_AVOID_LIST_EXPIRY_TIME);
	blm_cfg->black_list_exipry_time =
				cfg_get(psoc, CFG_BLACK_LIST_EXPIRY_TIME);
	blm_cfg->bad_bssid_counter_reset_time =
				cfg_get(psoc, CFG_BAD_BSSID_RESET_TIME);
	blm_cfg->bad_bssid_counter_thresh =
				cfg_get(psoc, CFG_BAD_BSSID_COUNTER_THRESHOLD);
	blm_cfg->delta_rssi =
				cfg_get(psoc, CFG_BLACKLIST_RSSI_THRESHOLD);
}

QDF_STATUS
blm_cfg_psoc_open(struct wlan_objmgr_psoc *psoc)
{
	struct blm_psoc_priv_obj *blm_psoc_obj;

	blm_psoc_obj = blm_get_psoc_obj(psoc);

	if (!blm_psoc_obj) {
		blm_err("BLM psoc obj NULL");
		return QDF_STATUS_E_FAILURE;
	}

	blm_init_cfg(psoc, &blm_psoc_obj->blm_cfg);

	return QDF_STATUS_SUCCESS;
}
