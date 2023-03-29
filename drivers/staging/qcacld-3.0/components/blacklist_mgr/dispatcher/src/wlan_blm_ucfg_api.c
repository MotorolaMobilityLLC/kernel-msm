/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
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
 * DOC: define UCFG APIs exposed by the blacklist mgr component
 */

#include <wlan_blm_ucfg_api.h>
#include <wlan_blm_core.h>
#include <wlan_blm_api.h>
#include "wlan_pmo_obj_mgmt_api.h"

QDF_STATUS ucfg_blm_init(void)
{
	QDF_STATUS status;

	status = wlan_objmgr_register_pdev_create_handler(
			WLAN_UMAC_COMP_BLACKLIST_MGR,
			blm_pdev_object_created_notification,
			NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		blm_err("pdev create register notification failed");
		goto fail_create_pdev;
	}

	status = wlan_objmgr_register_pdev_destroy_handler(
			WLAN_UMAC_COMP_BLACKLIST_MGR,
			blm_pdev_object_destroyed_notification,
			NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		blm_err("pdev destroy register notification failed");
		goto fail_destroy_pdev;
	}

	status = wlan_objmgr_register_psoc_create_handler(
			WLAN_UMAC_COMP_BLACKLIST_MGR,
			blm_psoc_object_created_notification,
			NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		blm_err("psoc create register notification failed");
		goto fail_create_psoc;
	}

	status = wlan_objmgr_register_psoc_destroy_handler(
			WLAN_UMAC_COMP_BLACKLIST_MGR,
			blm_psoc_object_destroyed_notification,
			NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		blm_err("psoc destroy register notification failed");
		goto fail_destroy_psoc;
	}

	return QDF_STATUS_SUCCESS;

fail_destroy_psoc:
	wlan_objmgr_unregister_psoc_create_handler(WLAN_UMAC_COMP_BLACKLIST_MGR,
				   blm_psoc_object_created_notification, NULL);
fail_create_psoc:
	wlan_objmgr_unregister_pdev_destroy_handler(
				 WLAN_UMAC_COMP_BLACKLIST_MGR,
				 blm_pdev_object_destroyed_notification, NULL);
fail_destroy_pdev:
	wlan_objmgr_unregister_pdev_create_handler(WLAN_UMAC_COMP_BLACKLIST_MGR,
				   blm_pdev_object_created_notification, NULL);
fail_create_pdev:
	return status;
}

QDF_STATUS ucfg_blm_deinit(void)
{
	QDF_STATUS status;

	status = wlan_objmgr_unregister_psoc_destroy_handler(
			WLAN_UMAC_COMP_BLACKLIST_MGR,
			blm_psoc_object_destroyed_notification,
			NULL);

	status = wlan_objmgr_unregister_psoc_create_handler(
			WLAN_UMAC_COMP_BLACKLIST_MGR,
			blm_psoc_object_created_notification,
			NULL);

	status = wlan_objmgr_unregister_pdev_destroy_handler(
			WLAN_UMAC_COMP_BLACKLIST_MGR,
			blm_pdev_object_destroyed_notification,
			NULL);

	status = wlan_objmgr_unregister_pdev_create_handler(
			WLAN_UMAC_COMP_BLACKLIST_MGR,
			blm_pdev_object_created_notification,
			NULL);

	return status;
}

QDF_STATUS ucfg_blm_psoc_set_suspended(struct wlan_objmgr_psoc *psoc,
				       bool state)
{
	struct blm_psoc_priv_obj *blm_psoc_obj;

	blm_psoc_obj = blm_get_psoc_obj(psoc);

	if (!blm_psoc_obj) {
		blm_err("BLM psoc obj NULL");
		return QDF_STATUS_E_FAILURE;
	}

	blm_psoc_obj->is_suspended = state;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_blm_psoc_get_suspended(struct wlan_objmgr_psoc *psoc,
				       bool *state)
{
	struct blm_psoc_priv_obj *blm_psoc_obj;

	blm_psoc_obj = blm_get_psoc_obj(psoc);

	if (!blm_psoc_obj) {
		blm_err("BLM psoc obj NULL");
		*state = true;
		return QDF_STATUS_E_FAILURE;
	}

	*state = blm_psoc_obj->is_suspended;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
ucfg_blm_suspend_handler(struct wlan_objmgr_psoc *psoc, void *arg)
{
	ucfg_blm_psoc_set_suspended(psoc, true);
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
ucfg_blm_resume_handler(struct wlan_objmgr_psoc *psoc, void *arg)
{
	ucfg_blm_psoc_set_suspended(psoc, false);
	blm_update_reject_ap_list_to_fw(psoc);
	return QDF_STATUS_SUCCESS;
}

static inline void
ucfg_blm_register_pmo_handler(void)
{
	pmo_register_suspend_handler(WLAN_UMAC_COMP_BLACKLIST_MGR,
				     ucfg_blm_suspend_handler, NULL);
	pmo_register_resume_handler(WLAN_UMAC_COMP_BLACKLIST_MGR,
				    ucfg_blm_resume_handler, NULL);
}

static inline void
ucfg_blm_unregister_pmo_handler(void)
{
	pmo_unregister_suspend_handler(WLAN_UMAC_COMP_BLACKLIST_MGR,
				       ucfg_blm_suspend_handler);
	pmo_unregister_resume_handler(WLAN_UMAC_COMP_BLACKLIST_MGR,
				      ucfg_blm_resume_handler);
}

QDF_STATUS ucfg_blm_psoc_open(struct wlan_objmgr_psoc *psoc)
{
	ucfg_blm_register_pmo_handler();
	return blm_cfg_psoc_open(psoc);
}

QDF_STATUS ucfg_blm_psoc_close(struct wlan_objmgr_psoc *psoc)
{
	ucfg_blm_unregister_pmo_handler();
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_blm_add_bssid_to_reject_list(struct wlan_objmgr_pdev *pdev,
				  struct reject_ap_info *ap_info)
{
	return blm_add_bssid_to_reject_list(pdev, ap_info);
}

QDF_STATUS
ucfg_blm_add_userspace_black_list(struct wlan_objmgr_pdev *pdev,
				  struct qdf_mac_addr *bssid_black_list,
				  uint8_t num_of_bssid)
{
	return blm_add_userspace_black_list(pdev, bssid_black_list,
					    num_of_bssid);
}

void
ucfg_blm_dump_black_list_ap(struct wlan_objmgr_pdev *pdev)
{
	return wlan_blm_dump_blcklist_bssid(pdev);
}

void
ucfg_blm_update_bssid_connect_params(struct wlan_objmgr_pdev *pdev,
				     struct qdf_mac_addr bssid,
				     enum blm_connection_state con_state)
{
	wlan_blm_update_bssid_connect_params(pdev, bssid, con_state);
}

void
ucfg_blm_wifi_off(struct wlan_objmgr_pdev *pdev)
{
	struct blm_pdev_priv_obj *blm_ctx;
	struct blm_psoc_priv_obj *blm_psoc_obj;
	struct blm_config *cfg;
	QDF_STATUS status;

	if (!pdev) {
		blm_err("pdev is NULL");
		return;
	}

	blm_ctx = blm_get_pdev_obj(pdev);
	blm_psoc_obj = blm_get_psoc_obj(wlan_pdev_get_psoc(pdev));

	if (!blm_ctx || !blm_psoc_obj) {
		blm_err("blm_ctx or blm_psoc_obj is NULL");
		return ;
	}

	status = qdf_mutex_acquire(&blm_ctx->reject_ap_list_lock);
	if (QDF_IS_STATUS_ERROR(status)) {
		blm_err("failed to acquire reject_ap_list_lock");
		return;
	}

	cfg = &blm_psoc_obj->blm_cfg;

	blm_flush_reject_ap_list(blm_ctx);
	blm_send_reject_ap_list_to_fw(pdev, &blm_ctx->reject_ap_list, cfg);
	qdf_mutex_release(&blm_ctx->reject_ap_list_lock);
}
