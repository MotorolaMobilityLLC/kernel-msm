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
 * DOC: define utility API related to the DISA component
 * called by other components
 */

#include "wlan_disa_obj_mgmt_api.h"
#include "wlan_disa_main.h"
#include "target_if_disa.h"
#include "wlan_disa_tgt_api.h"
#include "wlan_objmgr_global_obj.h"

/**
 * disa_init() - register disa notification handlers.
 *
 * This function registers disa related notification handlers and
 * allocates disa context.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success else return error
 */
QDF_STATUS disa_init(void)
{
	QDF_STATUS status;

	DISA_ENTER();

	if (disa_allocate_ctx() != QDF_STATUS_SUCCESS) {
		disa_err("unable to allocate disa ctx");
		status = QDF_STATUS_E_FAULT;
		goto out;
	}

	status = wlan_objmgr_register_psoc_create_handler(
			WLAN_UMAC_COMP_DISA,
			disa_psoc_object_created_notification,
			NULL);
	if (status != QDF_STATUS_SUCCESS) {
		disa_err("unable to register psoc create handler");
		goto err_free_ctx;
	}

	status = wlan_objmgr_register_psoc_destroy_handler(
			WLAN_UMAC_COMP_DISA,
			disa_psoc_object_destroyed_notification,
			NULL);
	if (status != QDF_STATUS_SUCCESS) {
		disa_err("unable to register psoc destroy handler");
		wlan_objmgr_unregister_psoc_create_handler(
				WLAN_UMAC_COMP_DISA,
				disa_psoc_object_created_notification,
				NULL);
	} else {
		goto out;
	}

err_free_ctx:
	disa_free_ctx();
out:
	DISA_EXIT();

	return status;
}

/**
 * disa_deinit() - unregister disa notification handlers.
 *
 * This function unregisters disa related notification handlers and
 * frees disa context.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success else return error
 */
QDF_STATUS disa_deinit(void)
{
	QDF_STATUS status;

	DISA_ENTER();
	status = wlan_objmgr_unregister_psoc_destroy_handler(
			WLAN_UMAC_COMP_DISA,
			disa_psoc_object_destroyed_notification,
			NULL);
	if (status != QDF_STATUS_SUCCESS)
		disa_err("unable to unregister psoc create handle");

	status = wlan_objmgr_unregister_psoc_create_handler(
			WLAN_UMAC_COMP_DISA,
			disa_psoc_object_created_notification,
			NULL);
	if (status != QDF_STATUS_SUCCESS)
		disa_err("unable to unregister psoc create handle");

	disa_free_ctx();
	DISA_EXIT();

	return status;
}

/**
 * disa_psoc_object_created_notification(): disa psoc create handler
 * @psoc: psoc which is going to created by objmgr
 * @arg: argument for psoc create handler
 *
 * Attach psoc private object, register rx/tx ops and event handlers
 *
 * Return QDF_STATUS status in case of success else return error
 */
QDF_STATUS disa_psoc_object_created_notification(
		struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct disa_psoc_priv_obj *disa_priv;
	QDF_STATUS status;

	DISA_ENTER();

	disa_priv = qdf_mem_malloc(sizeof(*disa_priv));
	if (!disa_priv) {
		status = QDF_STATUS_E_NOMEM;
		goto out;
	}

	status = wlan_objmgr_psoc_component_obj_attach(psoc,
			 WLAN_UMAC_COMP_DISA,
			(void *)disa_priv, QDF_STATUS_SUCCESS);
	if (status != QDF_STATUS_SUCCESS) {
		disa_err("Failed to attach disa_priv with psoc");
		qdf_mem_free(disa_priv);
		goto out;
	}

	qdf_spinlock_create(&disa_priv->lock);
	target_if_disa_register_tx_ops(&disa_priv->disa_tx_ops);

out:
	DISA_EXIT();

	return status;
}

/**
 * disa_psoc_object_destroyed_notification(): disa psoc destroy handler
 * @psoc: objmgr object corresponding to psoc which is going to be destroyed
 * @arg: argument for psoc destroy handler
 *
 * Detach and free psoc private object, unregister event handlers
 *
 * Return QDF_STATUS status in case of success else return error
 */
QDF_STATUS disa_psoc_object_destroyed_notification(
		struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct disa_psoc_priv_obj *disa_priv = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	DISA_ENTER();

	disa_priv = disa_psoc_get_priv(psoc);

	status = wlan_objmgr_psoc_component_obj_detach(psoc,
			 WLAN_UMAC_COMP_DISA,
			(void *)disa_priv);

	if (status != QDF_STATUS_SUCCESS)
		disa_err("Failed to detach disa_priv with psoc");

	qdf_spinlock_destroy(&disa_priv->lock);
	qdf_mem_free(disa_priv);
	DISA_EXIT();

	return status;
}

/**
 * disa_psoc_enable() - Trigger psoc enable for DISA
 * @psoc: objmgr psoc object
 *
 * Return: QDF status success or failure
 */
QDF_STATUS disa_psoc_enable(struct wlan_objmgr_psoc *psoc)
{
	return tgt_disa_register_ev_handlers(psoc);
}

/**
 * disa_psoc_disable() - Trigger psoc disable for DISA
 * @psoc: objmgr psoc object
 *
 * Return: QDF status success or failure
 */
QDF_STATUS disa_psoc_disable(struct wlan_objmgr_psoc *psoc)
{
	return tgt_disa_unregister_ev_handlers(psoc);

}

