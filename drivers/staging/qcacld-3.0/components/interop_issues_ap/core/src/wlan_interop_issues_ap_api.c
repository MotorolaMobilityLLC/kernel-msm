/*
 * Copyright (c) 2019 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_interop_issues_ap_api.c
 */
#include <wlan_interop_issues_ap_ucfg_api.h>
#include <wlan_interop_issues_ap_api.h>
#include <target_if_interop_issues_ap.h>

/**
 * interop_issues_ap_psoc_obj_created_notification() - PSOC obj create callback
 * @psoc: PSOC object
 * @arg_list: Variable argument list
 *
 * This callback is registered with object manager during initialization to
 * get notified when the object is created.
 *
 * Return: Success or Failure
 */
static QDF_STATUS
interop_issues_ap_psoc_obj_created_notification(struct wlan_objmgr_psoc *psoc,
						void *arg_list)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct interop_issues_ap_psoc_priv_obj *interop_issues_ap_obj;

	interop_issues_ap_obj = qdf_mem_malloc(sizeof(*interop_issues_ap_obj));
	if (!interop_issues_ap_obj)
		return QDF_STATUS_E_NOMEM;

	qdf_spinlock_create(&interop_issues_ap_obj->lock);
	status = wlan_objmgr_psoc_component_obj_attach(psoc,
					WLAN_UMAC_COMP_INTEROP_ISSUES_AP,
					interop_issues_ap_obj,
					QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		interop_issues_ap_err("obj attach with psoc failed");
		goto interop_issues_ap_psoc_attach_failed;
	}

	target_if_interop_issues_ap_register_tx_ops(psoc,
					&interop_issues_ap_obj->tx_ops);

	return QDF_STATUS_SUCCESS;

interop_issues_ap_psoc_attach_failed:
	qdf_spinlock_destroy(&interop_issues_ap_obj->lock);
	qdf_mem_free(interop_issues_ap_obj);
	return status;
}

/**
 * interop_issues_ap_psoc_obj_destroyed_notification() - obj delete callback
 * @psoc: PSOC object
 * @arg_list: Variable argument list
 *
 * This callback is registered with object manager during initialization to
 * get notified when the object is deleted.
 *
 * Return: Success or Failure
 */
static QDF_STATUS
interop_issues_ap_psoc_obj_destroyed_notification(struct wlan_objmgr_psoc *psoc,
						  void *arg_list)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct interop_issues_ap_psoc_priv_obj *interop_issues_ap_obj;

	interop_issues_ap_obj = interop_issues_ap_get_psoc_priv_obj(psoc);

	if (!interop_issues_ap_obj) {
		interop_issues_ap_err("interop_issues_ap_obj is NULL");
		return QDF_STATUS_E_FAULT;
	}
	target_if_interop_issues_ap_unregister_tx_ops(psoc,
					&interop_issues_ap_obj->tx_ops);

	status = wlan_objmgr_psoc_component_obj_detach(psoc,
					WLAN_UMAC_COMP_INTEROP_ISSUES_AP,
					interop_issues_ap_obj);
	if (QDF_IS_STATUS_ERROR(status))
		interop_issues_ap_err("interop_issues_ap_obj detach failed");

	qdf_spinlock_destroy(&interop_issues_ap_obj->lock);
	qdf_mem_free(interop_issues_ap_obj);

	return status;
}

QDF_STATUS wlan_interop_issues_ap_psoc_enable(struct wlan_objmgr_psoc *psoc)
{
	return target_if_interop_issues_ap_register_event_handler(psoc);
}

QDF_STATUS wlan_interop_issues_ap_psoc_disable(struct wlan_objmgr_psoc *psoc)
{
	return target_if_interop_issues_ap_unregister_event_handler(psoc);
}

QDF_STATUS wlan_interop_issues_ap_init(void)
{
	QDF_STATUS status;

	/* register psoc create handler functions. */
	status = wlan_objmgr_register_psoc_create_handler(
			WLAN_UMAC_COMP_INTEROP_ISSUES_AP,
			interop_issues_ap_psoc_obj_created_notification,
			NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		interop_issues_ap_err("register create handler failed");
		return status;
	}

	/* register psoc delete handler functions. */
	status = wlan_objmgr_register_psoc_destroy_handler(
			WLAN_UMAC_COMP_INTEROP_ISSUES_AP,
			interop_issues_ap_psoc_obj_destroyed_notification,
			NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		interop_issues_ap_err("register destroy handler failed");
		status = wlan_objmgr_unregister_psoc_create_handler(
				WLAN_UMAC_COMP_INTEROP_ISSUES_AP,
				interop_issues_ap_psoc_obj_created_notification,
				NULL);
	}

	return status;
}

QDF_STATUS wlan_interop_issues_ap_deinit(void)
{
	QDF_STATUS ret = QDF_STATUS_SUCCESS, status;

	/* unregister psoc delete handler functions. */
	status = wlan_objmgr_unregister_psoc_destroy_handler(
			WLAN_UMAC_COMP_INTEROP_ISSUES_AP,
			interop_issues_ap_psoc_obj_destroyed_notification,
			NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		interop_issues_ap_err("unregister destroy handler failed");
		ret = status;
	}

	/* unregister psoc create handler functions. */
	status = wlan_objmgr_unregister_psoc_create_handler(
			WLAN_UMAC_COMP_INTEROP_ISSUES_AP,
			interop_issues_ap_psoc_obj_created_notification,
			NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		interop_issues_ap_err("unregister create handler failed");
		ret = status;
	}

	return ret;
}
