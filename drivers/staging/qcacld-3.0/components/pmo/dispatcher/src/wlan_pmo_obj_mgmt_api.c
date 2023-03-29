/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
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
 * DOC: define utility API related to the pmo component
 * called by other components
 */

#include "wlan_pmo_obj_mgmt_api.h"
#include "wlan_pmo_tgt_api.h"
#include "wlan_pmo_static_config.h"
#include "wlan_pmo_main.h"
#include "target_if_pmo.h"

QDF_STATUS pmo_init(void)
{
	QDF_STATUS status;
	struct wlan_pmo_ctx *pmo_ctx;

	pmo_enter();
	if (pmo_allocate_ctx() != QDF_STATUS_SUCCESS) {
		pmo_err("unable to allocate psoc ctx");
		status = QDF_STATUS_E_FAULT;
		goto out;
	}

	pmo_ctx = pmo_get_context();
	if (!pmo_ctx) {
		pmo_err("unable to get pmo ctx");
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	status = wlan_objmgr_register_psoc_create_handler(
			WLAN_UMAC_COMP_PMO,
			pmo_psoc_object_created_notification,
			(void *)pmo_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("unable to register psoc create handle");
		goto out;
	}

	status = wlan_objmgr_register_psoc_destroy_handler(
			WLAN_UMAC_COMP_PMO,
			 pmo_psoc_object_destroyed_notification,
			(void *)pmo_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("unable to register psoc create handle");
		goto out;
	}

	status = wlan_objmgr_register_vdev_create_handler(
			WLAN_UMAC_COMP_PMO,
			pmo_vdev_object_created_notification,
			(void *)pmo_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("unable to register vdev create handle");
		goto out;
	}

	status = wlan_objmgr_register_vdev_destroy_handler(
			WLAN_UMAC_COMP_PMO,
			pmo_vdev_object_destroyed_notification,
			(void *)pmo_ctx);
	if (status != QDF_STATUS_SUCCESS)
		pmo_err("unable to register vdev create handle");
out:
	pmo_exit();

	return status;
}

QDF_STATUS pmo_deinit(void)
{
	QDF_STATUS status;
	struct wlan_pmo_ctx *pmo_ctx;

	pmo_enter();
	pmo_ctx = pmo_get_context();
	if (!pmo_ctx) {
		pmo_err("unable to get pmo ctx");
		status =  QDF_STATUS_E_FAILURE;
		goto out;
	}

	status = wlan_objmgr_unregister_psoc_create_handler(
			WLAN_UMAC_COMP_PMO,
			pmo_psoc_object_created_notification,
			(void *)pmo_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("unable to unregister psoc create handle");
		goto out;
	}

	status = wlan_objmgr_unregister_psoc_destroy_handler(
			WLAN_UMAC_COMP_PMO,
			 pmo_psoc_object_destroyed_notification,
			(void *)pmo_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("unable to unregister psoc create handle");
		goto out;
	}

	status = wlan_objmgr_unregister_vdev_create_handler(
			WLAN_UMAC_COMP_PMO,
			pmo_vdev_object_created_notification,
			(void *)pmo_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("unable to unregister vdev create handle");
		goto out;
	}

	status = wlan_objmgr_unregister_vdev_destroy_handler(
			WLAN_UMAC_COMP_PMO,
			pmo_vdev_object_destroyed_notification,
			(void *)pmo_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("unable to unregister vdev create handle");
		goto out;
	}

out:
	pmo_free_ctx();
	pmo_exit();

	return status;
}

QDF_STATUS pmo_psoc_object_created_notification(
		struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct pmo_psoc_priv_obj *psoc_ctx = NULL;
	QDF_STATUS status;
	struct wlan_pmo_ctx *pmo_ctx;

	pmo_enter();
	pmo_ctx = pmo_get_context();
	if (!pmo_ctx) {
		QDF_ASSERT(0);
		pmo_err("unable to get pmo ctx");
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}

	psoc_ctx = qdf_mem_malloc(sizeof(*psoc_ctx));
	if (!psoc_ctx) {
		status = QDF_STATUS_E_NOMEM;
		goto out;
	}

	status = wlan_objmgr_psoc_component_obj_attach(psoc,
			WLAN_UMAC_COMP_PMO,
			psoc_ctx,
			QDF_STATUS_SUCCESS);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("Failed to attach psoc_ctx with psoc");
		qdf_mem_free(psoc_ctx);
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}
	qdf_spinlock_create(&psoc_ctx->lock);
	qdf_wake_lock_create(&psoc_ctx->wow.wow_wake_lock, "pmo_wow_wl");
	status = qdf_event_create(&psoc_ctx->wow.target_suspend);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("target suspend event initialization failed");
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}
	status = qdf_event_create(&psoc_ctx->wow.target_resume);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("target resume event initialization failed");
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}

	qdf_atomic_init(&psoc_ctx->wow.wow_initial_wake_up);
	/* Register PMO tx ops*/
	target_if_pmo_register_tx_ops(&psoc_ctx->pmo_tx_ops);
out:
	pmo_exit();

	return status;
}

QDF_STATUS pmo_psoc_object_destroyed_notification(
		struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct pmo_psoc_priv_obj *psoc_ctx = NULL;
	QDF_STATUS status;

	pmo_enter();

	psoc_ctx = pmo_psoc_get_priv(psoc);

	status = wlan_objmgr_psoc_component_obj_detach(psoc,
			WLAN_UMAC_COMP_PMO,
			psoc_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("Failed to detach psoc_ctx from psoc");
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}

	qdf_spinlock_destroy(&psoc_ctx->lock);
	qdf_event_destroy(&psoc_ctx->wow.target_suspend);
	qdf_event_destroy(&psoc_ctx->wow.target_resume);
	qdf_wake_lock_destroy(&psoc_ctx->wow.wow_wake_lock);
	qdf_mem_zero(psoc_ctx, sizeof(*psoc_ctx));
	qdf_mem_free(psoc_ctx);
out:
	pmo_exit();

	return status;
}

QDF_STATUS pmo_vdev_object_created_notification(
		struct wlan_objmgr_vdev *vdev, void *arg)
{
	struct pmo_psoc_priv_obj *psoc_ctx = NULL;
	struct wlan_objmgr_psoc *psoc;
	struct pmo_vdev_priv_obj *vdev_ctx;
	QDF_STATUS status;

	pmo_enter();

	psoc = pmo_vdev_get_psoc(vdev);

	psoc_ctx = pmo_psoc_get_priv(psoc);

	vdev_ctx = qdf_mem_malloc(sizeof(*vdev_ctx));
	if (!vdev_ctx) {
		status = QDF_STATUS_E_NOMEM;
		goto out;
	}

	status = wlan_objmgr_vdev_component_obj_attach(vdev,
			 WLAN_UMAC_COMP_PMO,
			(void *)vdev_ctx, QDF_STATUS_SUCCESS);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("Failed to attach vdev_ctx with vdev");
		qdf_mem_free(vdev_ctx);
		goto out;
	}

	qdf_spinlock_create(&vdev_ctx->pmo_vdev_lock);
	vdev_ctx->ptrn_match_enable =
		psoc_ctx->psoc_cfg.ptrn_match_enable_all_vdev;
	vdev_ctx->pmo_psoc_ctx = psoc_ctx;
	qdf_atomic_init(&vdev_ctx->gtk_err_enable);

out:
	pmo_exit();

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS pmo_vdev_ready(struct wlan_objmgr_vdev *vdev)
{
	QDF_STATUS status;

	status = pmo_vdev_get_ref(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	/* Register static configuration with firmware */
	pmo_register_wow_wakeup_events(vdev);

	/* Register default wow patterns with firmware */
	pmo_register_wow_default_patterns(vdev);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);

	/*
	 * The above APIs should return a status but don't.
	 * Just return success for now.
	 */
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS pmo_vdev_object_destroyed_notification(
		struct wlan_objmgr_vdev *vdev, void *arg)
{
	struct pmo_vdev_priv_obj *vdev_ctx = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	vdev_ctx = pmo_vdev_get_priv(vdev);

	status = wlan_objmgr_vdev_component_obj_detach(vdev,
			 WLAN_UMAC_COMP_PMO,
			(void *)vdev_ctx);
	if (status != QDF_STATUS_SUCCESS)
		pmo_err("Failed to detach vdev_ctx with vdev");

	qdf_spinlock_destroy(&vdev_ctx->pmo_vdev_lock);
	qdf_mem_free(vdev_ctx);

	return status;
}

QDF_STATUS pmo_register_suspend_handler(
		enum wlan_umac_comp_id id,
		pmo_psoc_suspend_handler handler,
		void *arg)
{
	struct wlan_pmo_ctx *pmo_ctx;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	pmo_enter();
	pmo_ctx = pmo_get_context();
	if (!pmo_ctx) {
		QDF_ASSERT(0);
		pmo_err("unable to get pmo ctx");
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}

	if (id < 0 || id >= WLAN_UMAC_MAX_COMPONENTS) {
		pmo_err("component id: %d is %s then valid components id",
			id, id < 0 ? "Less" : "More");
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}

	qdf_spin_lock_bh(&pmo_ctx->lock);
	pmo_ctx->pmo_suspend_handler[id] = handler;
	pmo_ctx->pmo_suspend_handler_arg[id] = arg;
	qdf_spin_unlock_bh(&pmo_ctx->lock);
out:
	pmo_exit();

	return status;
}

QDF_STATUS pmo_unregister_suspend_handler(
		enum wlan_umac_comp_id id,
		pmo_psoc_suspend_handler handler)
{
	struct wlan_pmo_ctx *pmo_ctx;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	pmo_enter();
	pmo_ctx = pmo_get_context();
	if (!pmo_ctx) {
		QDF_ASSERT(0);
		pmo_err("unable to get pmo ctx");
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}

	if (id < 0 || id >= WLAN_UMAC_MAX_COMPONENTS) {
		pmo_err("component id: %d is %s then valid components id",
			id, id < 0 ? "Less" : "More");
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}

	qdf_spin_lock_bh(&pmo_ctx->lock);
	if (pmo_ctx->pmo_suspend_handler[id] == handler) {
		pmo_ctx->pmo_suspend_handler[id] = NULL;
		pmo_ctx->pmo_suspend_handler_arg[id] = NULL;
		qdf_spin_unlock_bh(&pmo_ctx->lock);
	} else {
		qdf_spin_unlock_bh(&pmo_ctx->lock);
		pmo_err("can't find suspend handler for component id: %d ", id);
		status = QDF_STATUS_E_FAILURE;
	}
out:
	pmo_exit();

	return status;
}

QDF_STATUS pmo_register_resume_handler(
		enum wlan_umac_comp_id id,
		pmo_psoc_resume_handler handler,
		void *arg)
{
	struct wlan_pmo_ctx *pmo_ctx;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	pmo_enter();
	pmo_ctx = pmo_get_context();
	if (!pmo_ctx) {
		pmo_err("unable to get pmo ctx");
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}

	if (id < 0 || id >= WLAN_UMAC_MAX_COMPONENTS) {
		pmo_err("component id: %d is %s then valid components id",
			id, id < 0 ? "Less" : "More");
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}

	qdf_spin_lock_bh(&pmo_ctx->lock);
	pmo_ctx->pmo_resume_handler[id] = handler;
	pmo_ctx->pmo_resume_handler_arg[id] = arg;
	qdf_spin_unlock_bh(&pmo_ctx->lock);
out:
	pmo_exit();

	return status;
}

QDF_STATUS pmo_unregister_resume_handler(
		enum wlan_umac_comp_id id,
		pmo_psoc_resume_handler handler)
{
	struct wlan_pmo_ctx *pmo_ctx;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	pmo_enter();
	pmo_ctx = pmo_get_context();
	if (!pmo_ctx) {
		pmo_err("unable to get pmo ctx");
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}

	if (id < 0 || id >= WLAN_UMAC_MAX_COMPONENTS) {
		pmo_err("component id: %d is %s then valid components id",
			id, id < 0 ? "Less" : "More");
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}

	qdf_spin_lock_bh(&pmo_ctx->lock);
	if (pmo_ctx->pmo_resume_handler[id] == handler) {
		pmo_ctx->pmo_resume_handler[id] = NULL;
		pmo_ctx->pmo_resume_handler_arg[id] = NULL;
		qdf_spin_unlock_bh(&pmo_ctx->lock);
	} else {
		qdf_spin_unlock_bh(&pmo_ctx->lock);
		pmo_err("can't find resume handler for component id: %d ", id);
		status = QDF_STATUS_E_FAILURE;
	}
out:
	pmo_exit();

	return status;
}

QDF_STATUS pmo_suspend_all_components(struct wlan_objmgr_psoc *psoc,
				      enum qdf_suspend_type suspend_type)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	QDF_STATUS resume_status;
	struct wlan_pmo_ctx *pmo_ctx;
	int i;
	pmo_psoc_suspend_handler handler;
	void *arg;

	pmo_enter();

	pmo_ctx = pmo_get_context();
	if (!pmo_ctx) {
		pmo_err("unable to get pmo ctx");
		QDF_ASSERT(0);
		status = QDF_STATUS_E_FAILURE;
		goto exit_with_status;
	}

	/* call each component's suspend handler */
	for (i = 0; i < WLAN_UMAC_MAX_COMPONENTS; i++) {
		qdf_spin_lock_bh(&pmo_ctx->lock);
		handler = pmo_ctx->pmo_suspend_handler[i];
		arg = pmo_ctx->pmo_suspend_handler_arg[i];
		qdf_spin_unlock_bh(&pmo_ctx->lock);

		if (!handler)
			continue;

		status = handler(psoc, arg);
		if (QDF_IS_STATUS_ERROR(status)) {
			pmo_err("component %d failed to suspend; status: %d",
				i, status);
			QDF_ASSERT(0);
			goto suspend_recovery;
		}
	}

	goto exit_with_status;

suspend_recovery:
	/* resume, starting with the last successfully suspended component */
	for (i -= 1; i >= 0; i--) {
		qdf_spin_lock_bh(&pmo_ctx->lock);
		handler = pmo_ctx->pmo_resume_handler[i];
		arg = pmo_ctx->pmo_resume_handler_arg[i];
		qdf_spin_unlock_bh(&pmo_ctx->lock);

		if (!handler)
			continue;

		resume_status = handler(psoc, arg);
		if (QDF_IS_STATUS_ERROR(resume_status))
			QDF_DEBUG_PANIC("component %d failed resume; status:%d",
					i, resume_status);
	}

exit_with_status:
	pmo_exit();

	return status;
}

QDF_STATUS pmo_resume_all_components(struct wlan_objmgr_psoc *psoc,
				     enum qdf_suspend_type suspend_type)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_pmo_ctx *pmo_ctx;
	uint8_t i;
	pmo_psoc_suspend_handler handler;
	void *arg;

	pmo_enter();

	pmo_ctx = pmo_get_context();
	if (!pmo_ctx) {
		pmo_err("unable to get pmo ctx");
		QDF_ASSERT(0);
		status = QDF_STATUS_E_FAILURE;
		goto exit_with_status;
	}

	/* call each component's resume handler */
	for (i = 0; i < WLAN_UMAC_MAX_COMPONENTS; i++) {
		qdf_spin_lock_bh(&pmo_ctx->lock);
		handler = pmo_ctx->pmo_resume_handler[i];
		arg = pmo_ctx->pmo_resume_handler_arg[i];
		qdf_spin_unlock_bh(&pmo_ctx->lock);

		if (!handler)
			continue;

		status = handler(psoc, arg);
		if (QDF_IS_STATUS_ERROR(status)) {
			pmo_fatal("Non-recoverable failure occurred!");
			pmo_fatal("component %d failed to resume; status: %d",
				  i, status);
			QDF_BUG(0);
		}
	}

exit_with_status:
	pmo_exit();

	return status;
}

QDF_STATUS pmo_register_pause_bitmap_notifier(struct wlan_objmgr_psoc *psoc,
		pmo_notify_pause_bitmap handler)
{
	struct pmo_psoc_priv_obj *psoc_ctx;
	QDF_STATUS status;

	if (!psoc) {
		pmo_err("psoc is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!handler) {
		pmo_err("pmo_notify_vdev_pause_bitmap is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = pmo_psoc_get_ref(psoc);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("pmo cannot get the reference out of psoc");
		return status;
	}

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		psoc_ctx->pause_bitmap_notifier = handler;
	}

	pmo_psoc_put_ref(psoc);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS pmo_unregister_pause_bitmap_notifier(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *psoc_ctx;
	QDF_STATUS status;

	if (!psoc) {
		pmo_err("psoc is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = pmo_psoc_get_ref(psoc);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("pmo cannot get the reference out of psoc");
		return status;
	}

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		psoc_ctx->pause_bitmap_notifier = NULL;
	}

	pmo_psoc_put_ref(psoc);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS pmo_register_get_pause_bitmap(struct wlan_objmgr_psoc *psoc,
		pmo_get_pause_bitmap handler)
{
	struct pmo_psoc_priv_obj *psoc_ctx;
	QDF_STATUS status;

	if (!psoc) {
		pmo_err("psoc is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!handler) {
		pmo_err("pmo_get_pause_bitmap is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = pmo_psoc_get_ref(psoc);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("pmo cannot get the reference out of psoc");
		return status;
	}

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		psoc_ctx->get_pause_bitmap = handler;
	}

	pmo_psoc_put_ref(psoc);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS pmo_unregister_get_pause_bitmap(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *psoc_ctx;
	QDF_STATUS status;

	if (!psoc) {
		pmo_err("psoc is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = pmo_psoc_get_ref(psoc);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("pmo cannot get the reference out of psoc");
		return status;
	}

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		psoc_ctx->get_pause_bitmap = NULL;
	}

	pmo_psoc_put_ref(psoc);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS pmo_register_is_device_in_low_pwr_mode(struct wlan_objmgr_psoc *psoc,
		pmo_is_device_in_low_pwr_mode handler)
{
	struct pmo_psoc_priv_obj *psoc_ctx;
	QDF_STATUS status;

	if (!psoc) {
		pmo_err("psoc is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!handler) {
		pmo_err("pmo_get_pause_bitmap is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = pmo_psoc_get_ref(psoc);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("pmo cannot get the reference out of psoc");
		return status;
	}

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		psoc_ctx->is_device_in_low_pwr_mode = handler;
	}

	pmo_psoc_put_ref(psoc);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
pmo_unregister_is_device_in_low_pwr_mode(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *psoc_ctx;
	QDF_STATUS status;

	if (!psoc) {
		pmo_err("psoc is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = pmo_psoc_get_ref(psoc);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("pmo cannot get the reference out of psoc");
		return status;
	}

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		psoc_ctx->is_device_in_low_pwr_mode = NULL;
	}

	pmo_psoc_put_ref(psoc);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS pmo_register_get_dtim_period_callback(struct wlan_objmgr_psoc *psoc,
						 pmo_get_dtim_period handler)
{
	struct pmo_psoc_priv_obj *psoc_ctx;
	QDF_STATUS status;

	if (!psoc) {
		pmo_err("psoc is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!handler) {
		pmo_err("pmo_get_dtim_period is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = pmo_psoc_get_ref(psoc);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("pmo cannot get the reference out of psoc");
		return status;
	}

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		psoc_ctx->get_dtim_period = handler;
	}
	pmo_psoc_put_ref(psoc);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
pmo_unregister_get_dtim_period_callback(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *psoc_ctx;
	QDF_STATUS status;

	if (!psoc) {
		pmo_err("psoc is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = pmo_psoc_get_ref(psoc);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("pmo cannot get the reference out of psoc");
		return status;
	}

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		psoc_ctx->get_dtim_period = NULL;
	}
	pmo_psoc_put_ref(psoc);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
pmo_register_get_beacon_interval_callback(struct wlan_objmgr_psoc *psoc,
					  pmo_get_beacon_interval handler)
{
	struct pmo_psoc_priv_obj *psoc_ctx;
	QDF_STATUS status;

	if (!psoc) {
		pmo_err("psoc is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!handler) {
		pmo_err("pmo_get_beacon_interval is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = pmo_psoc_get_ref(psoc);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("pmo cannot get the reference out of psoc");
		return status;
	}

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		psoc_ctx->get_beacon_interval = handler;
	}
	pmo_psoc_put_ref(psoc);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
pmo_unregister_get_beacon_interval_callback(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *psoc_ctx;
	QDF_STATUS status;

	if (!psoc) {
		pmo_err("psoc is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = pmo_psoc_get_ref(psoc);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("pmo cannot get the reference out of psoc");
		return status;
	}

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		psoc_ctx->get_beacon_interval = NULL;
	}
	pmo_psoc_put_ref(psoc);

	return QDF_STATUS_SUCCESS;
}

bool
wlan_pmo_get_sap_mode_bus_suspend(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	if (!pmo_psoc_ctx)
		return false;

	return pmo_psoc_ctx->psoc_cfg.is_bus_suspend_enabled_in_sap_mode;
}

bool
wlan_pmo_get_go_mode_bus_suspend(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	if (!pmo_psoc_ctx)
		return false;

	return pmo_psoc_ctx->psoc_cfg.is_bus_suspend_enabled_in_go_mode;
}
