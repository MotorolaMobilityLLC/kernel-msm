/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
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
 * DOC: contains nan public API function definitions
 */

#include "nan_main_i.h"
#include "wlan_nan_api.h"
#include "target_if_nan.h"
#include "nan_public_structs.h"
#include "wlan_objmgr_cmn.h"
#include "wlan_objmgr_global_obj.h"
#include "wlan_objmgr_psoc_obj.h"
#include "wlan_objmgr_pdev_obj.h"
#include "wlan_objmgr_vdev_obj.h"
#include "nan_ucfg_api.h"
#include <wlan_mlme_api.h>

static QDF_STATUS nan_psoc_obj_created_notification(
		struct wlan_objmgr_psoc *psoc, void *arg_list)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct nan_psoc_priv_obj *nan_obj;

	nan_debug("nan_psoc_create_notif called");
	nan_obj = qdf_mem_malloc(sizeof(*nan_obj));
	if (!nan_obj)
		return QDF_STATUS_E_NOMEM;

	qdf_spinlock_create(&nan_obj->lock);
	status = wlan_objmgr_psoc_component_obj_attach(psoc, WLAN_UMAC_COMP_NAN,
						       nan_obj,
						       QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		nan_alert("obj attach with psoc failed");
		goto nan_psoc_notif_failed;
	}

	target_if_nan_register_tx_ops(&nan_obj->tx_ops);
	target_if_nan_register_rx_ops(&nan_obj->rx_ops);

	return QDF_STATUS_SUCCESS;

nan_psoc_notif_failed:

	qdf_spinlock_destroy(&nan_obj->lock);
	qdf_mem_free(nan_obj);
	return status;
}

static QDF_STATUS nan_psoc_obj_destroyed_notification(
				struct wlan_objmgr_psoc *psoc, void *arg_list)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct nan_psoc_priv_obj *nan_obj = nan_get_psoc_priv_obj(psoc);

	nan_debug("nan_psoc_delete_notif called");
	if (!nan_obj) {
		nan_err("nan_obj is NULL");
		return QDF_STATUS_E_FAULT;
	}

	status = wlan_objmgr_psoc_component_obj_detach(psoc,
						       WLAN_UMAC_COMP_NAN,
						       nan_obj);
	if (QDF_IS_STATUS_ERROR(status))
		nan_err("nan_obj detach failed");

	nan_debug("nan_obj deleted with status %d", status);
	qdf_spinlock_destroy(&nan_obj->lock);
	qdf_mem_free(nan_obj);

	return status;
}

static QDF_STATUS nan_vdev_obj_created_notification(
		struct wlan_objmgr_vdev *vdev, void *arg_list)
{
	struct nan_vdev_priv_obj *nan_obj;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_objmgr_psoc *psoc;

	nan_debug("nan_vdev_create_notif called");
	if (ucfg_is_nan_vdev(vdev)) {
		psoc = wlan_vdev_get_psoc(vdev);
		if (!psoc) {
			nan_err("psoc is NULL");
			return QDF_STATUS_E_INVAL;
		}
		target_if_nan_set_vdev_feature_config(psoc,
						      wlan_vdev_get_id(vdev));
	}
	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_NDI_MODE) {
		nan_debug("not a ndi vdev. do nothing");
		return QDF_STATUS_SUCCESS;
	}

	nan_obj = qdf_mem_malloc(sizeof(*nan_obj));
	if (!nan_obj)
		return QDF_STATUS_E_NOMEM;

	qdf_spinlock_create(&nan_obj->lock);
	status = wlan_objmgr_vdev_component_obj_attach(vdev, WLAN_UMAC_COMP_NAN,
						       (void *)nan_obj,
						       QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		nan_alert("obj attach with vdev failed");
		goto nan_vdev_notif_failed;
	}

	return QDF_STATUS_SUCCESS;

nan_vdev_notif_failed:

	qdf_spinlock_destroy(&nan_obj->lock);
	qdf_mem_free(nan_obj);
	return status;
}

static QDF_STATUS nan_vdev_obj_destroyed_notification(
				struct wlan_objmgr_vdev *vdev, void *arg_list)
{
	struct nan_vdev_priv_obj *nan_obj;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	nan_debug("nan_vdev_delete_notif called");
	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_NDI_MODE) {
		nan_debug("not a ndi vdev. do nothing");
		return QDF_STATUS_SUCCESS;
	}

	nan_obj = nan_get_vdev_priv_obj(vdev);
	if (!nan_obj) {
		nan_err("nan_obj is NULL");
		return QDF_STATUS_E_FAULT;
	}

	status = wlan_objmgr_vdev_component_obj_detach(vdev, WLAN_UMAC_COMP_NAN,
						       nan_obj);
	if (QDF_IS_STATUS_ERROR(status))
		nan_err("nan_obj detach failed");

	nan_debug("nan_obj deleted with status %d", status);
	qdf_spinlock_destroy(&nan_obj->lock);
	qdf_mem_free(nan_obj);

	return status;
}

/**
 * nan_peer_obj_created_notification() - Handler for peer object creation
 * notification event
 * @peer: Pointer to the PEER Object
 * @arg_list: Pointer to private argument - NULL
 *
 * This function gets called from object manager when peer is being
 * created.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS nan_peer_obj_created_notification(
		struct wlan_objmgr_peer *peer, void *arg_list)
{
	struct nan_peer_priv_obj *nan_peer_obj;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	nan_peer_obj = qdf_mem_malloc(sizeof(*nan_peer_obj));
	if (!nan_peer_obj)
		return QDF_STATUS_E_NOMEM;

	qdf_spinlock_create(&nan_peer_obj->lock);
	status = wlan_objmgr_peer_component_obj_attach(peer, WLAN_UMAC_COMP_NAN,
						       (void *)nan_peer_obj,
						       QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		nan_alert("obj attach with peer failed");
		goto nan_peer_notif_failed;
	}

	return QDF_STATUS_SUCCESS;

nan_peer_notif_failed:

	qdf_spinlock_destroy(&nan_peer_obj->lock);
	qdf_mem_free(nan_peer_obj);
	return status;
}

/**
 * nan_peer_obj_destroyed_notification() - Handler for peer object deletion
 * notification event
 * @peer: Pointer to the PEER Object
 * @arg_list: Pointer to private argument - NULL
 *
 * This function gets called from object manager when peer is being destroyed.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS nan_peer_obj_destroyed_notification(
				struct wlan_objmgr_peer *peer, void *arg_list)
{
	struct nan_peer_priv_obj *nan_peer_obj;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	nan_peer_obj = nan_get_peer_priv_obj(peer);
	if (!nan_peer_obj) {
		nan_err("nan_peer_obj is NULL");
		return QDF_STATUS_E_FAULT;
	}

	status = wlan_objmgr_peer_component_obj_detach(peer, WLAN_UMAC_COMP_NAN,
						       nan_peer_obj);
	if (QDF_IS_STATUS_ERROR(status))
		nan_err("nan_peer_obj detach failed");

	nan_debug("nan_peer_obj deleted with status %d", status);
	qdf_spinlock_destroy(&nan_peer_obj->lock);
	qdf_mem_free(nan_peer_obj);

	return status;
}

QDF_STATUS nan_init(void)
{
	QDF_STATUS status;

	/* register psoc create handler functions. */
	status = wlan_objmgr_register_psoc_create_handler(
		WLAN_UMAC_COMP_NAN,
		nan_psoc_obj_created_notification,
		NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		nan_err("wlan_objmgr_register_psoc_create_handler failed");
		return status;
	}

	/* register psoc delete handler functions. */
	status = wlan_objmgr_register_psoc_destroy_handler(
		WLAN_UMAC_COMP_NAN,
		nan_psoc_obj_destroyed_notification,
		NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		nan_err("wlan_objmgr_register_psoc_destroy_handler failed");
		goto err_psoc_destroy_reg;
	}

	/* register vdev create handler functions. */
	status = wlan_objmgr_register_vdev_create_handler(
		WLAN_UMAC_COMP_NAN,
		nan_vdev_obj_created_notification,
		NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		nan_err("wlan_objmgr_register_psoc_create_handler failed");
		goto err_vdev_create_reg;
	}

	/* register vdev delete handler functions. */
	status = wlan_objmgr_register_vdev_destroy_handler(
		WLAN_UMAC_COMP_NAN,
		nan_vdev_obj_destroyed_notification,
		NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		nan_err("wlan_objmgr_register_psoc_destroy_handler failed");
		goto err_vdev_destroy_reg;
	}

	/* register peer create handler functions. */
	status = wlan_objmgr_register_peer_create_handler(
		WLAN_UMAC_COMP_NAN,
		nan_peer_obj_created_notification,
		NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		nan_err("wlan_objmgr_register_peer_create_handler failed");
		goto err_peer_create_reg;
	}

	/* register peer delete handler functions. */
	status = wlan_objmgr_register_peer_destroy_handler(
		WLAN_UMAC_COMP_NAN,
		nan_peer_obj_destroyed_notification,
		NULL);
	if (QDF_IS_STATUS_ERROR(status))
		nan_err("wlan_objmgr_register_peer_destroy_handler failed");
	else
		return QDF_STATUS_SUCCESS;

	wlan_objmgr_unregister_peer_create_handler(WLAN_UMAC_COMP_NAN,
					nan_peer_obj_created_notification,
					NULL);
err_peer_create_reg:
	wlan_objmgr_unregister_vdev_destroy_handler(WLAN_UMAC_COMP_NAN,
					nan_vdev_obj_destroyed_notification,
					NULL);
err_vdev_destroy_reg:
	wlan_objmgr_unregister_vdev_create_handler(WLAN_UMAC_COMP_NAN,
					nan_vdev_obj_created_notification,
					NULL);
err_vdev_create_reg:
	wlan_objmgr_unregister_psoc_destroy_handler(WLAN_UMAC_COMP_NAN,
					nan_psoc_obj_destroyed_notification,
					NULL);
err_psoc_destroy_reg:
	wlan_objmgr_unregister_psoc_create_handler(WLAN_UMAC_COMP_NAN,
					nan_psoc_obj_created_notification,
					NULL);

	return status;
}

QDF_STATUS nan_deinit(void)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS, status;

	/* register psoc create handler functions. */
	status = wlan_objmgr_unregister_psoc_create_handler(
		WLAN_UMAC_COMP_NAN,
		nan_psoc_obj_created_notification,
		NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		nan_err("wlan_objmgr_unregister_psoc_create_handler failed");
		ret = status;
	}

	/* register vdev create handler functions. */
	status = wlan_objmgr_unregister_psoc_destroy_handler(
		WLAN_UMAC_COMP_NAN,
		nan_psoc_obj_destroyed_notification,
		NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		nan_err("wlan_objmgr_deregister_psoc_destroy_handler failed");
		ret = status;
	}

	/* de-register vdev create handler functions. */
	status = wlan_objmgr_unregister_vdev_create_handler(
		WLAN_UMAC_COMP_NAN,
		nan_vdev_obj_created_notification,
		NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		nan_err("wlan_objmgr_unregister_psoc_create_handler failed");
		ret = status;
	}

	/* de-register vdev delete handler functions. */
	status = wlan_objmgr_unregister_vdev_destroy_handler(
		WLAN_UMAC_COMP_NAN,
		nan_vdev_obj_destroyed_notification,
		NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		nan_err("wlan_objmgr_deregister_psoc_destroy_handler failed");
		ret = status;
	}

	/* de-register peer create handler functions. */
	status = wlan_objmgr_unregister_peer_create_handler(
		WLAN_UMAC_COMP_NAN,
		nan_peer_obj_created_notification,
		NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		nan_err("wlan_objmgr_unregister_peer_create_handler failed");
		ret = status;
	}

	/* de-register peer delete handler functions. */
	status = wlan_objmgr_unregister_peer_destroy_handler(
		WLAN_UMAC_COMP_NAN,
		nan_peer_obj_destroyed_notification,
		NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		nan_err("wlan_objmgr_deregister_peer_destroy_handler failed");
		ret = status;
	}

	return ret;
}

QDF_STATUS nan_psoc_enable(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status = target_if_nan_register_events(psoc);

	if (QDF_IS_STATUS_ERROR(status))
		nan_err("target_if_nan_register_events failed");

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS nan_psoc_disable(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status = target_if_nan_deregister_events(psoc);

	if (QDF_IS_STATUS_ERROR(status))
		nan_err("target_if_nan_deregister_events failed");

	return QDF_STATUS_SUCCESS;
}

bool wlan_is_nan_allowed_on_freq(struct wlan_objmgr_pdev *pdev, uint32_t freq)
{
	bool nan_allowed = true;

	/* Check for SRD channels */
	if (wlan_reg_is_etsi13_srd_chan_for_freq(pdev, freq))
		wlan_mlme_get_srd_master_mode_for_vdev(wlan_pdev_get_psoc(pdev),
						       QDF_NAN_DISC_MODE,
						       &nan_allowed);

	/* Check for Indoor channels */
	if (wlan_reg_is_freq_indoor(pdev, freq))
		wlan_mlme_get_indoor_support_for_nan(wlan_pdev_get_psoc(pdev),
						     &nan_allowed);
	/* Check for dfs only if channel is not indoor */
	else if (wlan_reg_is_dfs_for_freq(pdev, freq))
		nan_allowed = false;

	return nan_allowed;
}
