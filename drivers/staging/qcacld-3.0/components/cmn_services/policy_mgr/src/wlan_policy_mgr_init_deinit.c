/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_policy_mgr_init_deinit.c
 *
 * WLAN Concurrenct Connection Management APIs
 *
 */

/* Include files */

#include "wlan_policy_mgr_api.h"
#include "wlan_policy_mgr_tables_no_dbs_i.h"
#include "wlan_policy_mgr_tables_1x1_dbs_i.h"
#include "wlan_policy_mgr_tables_2x2_dbs_i.h"
#include "wlan_policy_mgr_tables_2x2_5g_1x1_2g.h"
#include "wlan_policy_mgr_tables_2x2_2g_1x1_5g.h"
#include "wlan_policy_mgr_i.h"
#include "qdf_types.h"
#include "qdf_trace.h"
#include "wlan_objmgr_global_obj.h"
#include "target_if.h"

static QDF_STATUS policy_mgr_psoc_obj_create_cb(struct wlan_objmgr_psoc *psoc,
		void *data)
{
	struct policy_mgr_psoc_priv_obj *policy_mgr_ctx;

	policy_mgr_ctx = qdf_mem_malloc(
		sizeof(struct policy_mgr_psoc_priv_obj));
	if (!policy_mgr_ctx)
		return QDF_STATUS_E_FAILURE;

	policy_mgr_ctx->psoc = psoc;
	policy_mgr_ctx->old_hw_mode_index = POLICY_MGR_DEFAULT_HW_MODE_INDEX;
	policy_mgr_ctx->new_hw_mode_index = POLICY_MGR_DEFAULT_HW_MODE_INDEX;

	wlan_objmgr_psoc_component_obj_attach(psoc,
			WLAN_UMAC_COMP_POLICY_MGR,
			policy_mgr_ctx,
			QDF_STATUS_SUCCESS);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS policy_mgr_psoc_obj_destroy_cb(struct wlan_objmgr_psoc *psoc,
		void *data)
{
	struct policy_mgr_psoc_priv_obj *policy_mgr_ctx;

	policy_mgr_ctx = policy_mgr_get_context(psoc);
	wlan_objmgr_psoc_component_obj_detach(psoc,
					WLAN_UMAC_COMP_POLICY_MGR,
					policy_mgr_ctx);
	qdf_mem_free(policy_mgr_ctx);

	return QDF_STATUS_SUCCESS;
}

static void policy_mgr_psoc_obj_status_cb(struct wlan_objmgr_psoc *psoc,
		void *data, QDF_STATUS status)
{
	return;
}

static QDF_STATUS policy_mgr_pdev_obj_create_cb(struct wlan_objmgr_pdev *pdev,
		void *data)
{
	struct policy_mgr_psoc_priv_obj *policy_mgr_ctx;
	struct wlan_objmgr_psoc *psoc;

	psoc = wlan_pdev_get_psoc(pdev);
	policy_mgr_ctx = policy_mgr_get_context(psoc);
	if (!policy_mgr_ctx) {
		policy_mgr_err("invalid context");
		return QDF_STATUS_E_FAILURE;
	}

	policy_mgr_ctx->pdev = pdev;

	wlan_reg_register_chan_change_callback(psoc,
		policy_mgr_reg_chan_change_callback, NULL);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS policy_mgr_pdev_obj_destroy_cb(struct wlan_objmgr_pdev *pdev,
		void *data)
{
	struct policy_mgr_psoc_priv_obj *policy_mgr_ctx;
	struct wlan_objmgr_psoc *psoc;

	psoc = wlan_pdev_get_psoc(pdev);
	policy_mgr_ctx = policy_mgr_get_context(psoc);
	if (!policy_mgr_ctx) {
		policy_mgr_err("invalid context");
		return QDF_STATUS_E_FAILURE;
	}

	policy_mgr_ctx->pdev = NULL;
	wlan_reg_unregister_chan_change_callback(psoc,
		policy_mgr_reg_chan_change_callback);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS policy_mgr_vdev_obj_create_cb(struct wlan_objmgr_vdev *vdev,
		void *data)
{
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS policy_mgr_vdev_obj_destroy_cb(struct wlan_objmgr_vdev *vdev,
		void *data)
{
	return QDF_STATUS_SUCCESS;
}

static void policy_mgr_vdev_obj_status_cb(struct wlan_objmgr_vdev *vdev,
		void *data, QDF_STATUS status)
{
	return;
}

QDF_STATUS policy_mgr_init(void)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	status = wlan_objmgr_register_psoc_create_handler(
				WLAN_UMAC_COMP_POLICY_MGR,
				policy_mgr_psoc_obj_create_cb,
				NULL);
	if (status != QDF_STATUS_SUCCESS) {
		policy_mgr_err("Failed to register psoc obj create cback");
		goto err_psoc_create;
	}

	status = wlan_objmgr_register_psoc_destroy_handler(
				WLAN_UMAC_COMP_POLICY_MGR,
				policy_mgr_psoc_obj_destroy_cb,
				NULL);
	if (status != QDF_STATUS_SUCCESS) {
		policy_mgr_err("Failed to register psoc obj delete cback");
		goto err_psoc_delete;
	}

	status = wlan_objmgr_register_psoc_status_handler(
				WLAN_UMAC_COMP_POLICY_MGR,
				policy_mgr_psoc_obj_status_cb,
				NULL);
	if (status != QDF_STATUS_SUCCESS) {
		policy_mgr_err("Failed to register psoc obj status cback");
		goto err_psoc_status;
	}

	status = wlan_objmgr_register_pdev_create_handler(
				WLAN_UMAC_COMP_POLICY_MGR,
				policy_mgr_pdev_obj_create_cb,
				NULL);
	if (status != QDF_STATUS_SUCCESS) {
		policy_mgr_err("Failed to register pdev obj create cback");
		goto err_pdev_create;
	}

	status = wlan_objmgr_register_pdev_destroy_handler(
				WLAN_UMAC_COMP_POLICY_MGR,
				policy_mgr_pdev_obj_destroy_cb,
				NULL);
	if (status != QDF_STATUS_SUCCESS) {
		policy_mgr_err("Failed to register pdev obj delete cback");
		goto err_pdev_delete;
	}

	status = wlan_objmgr_register_vdev_create_handler(
				WLAN_UMAC_COMP_POLICY_MGR,
				policy_mgr_vdev_obj_create_cb,
				NULL);
	if (status != QDF_STATUS_SUCCESS) {
		policy_mgr_err("Failed to register vdev obj create cback");
		goto err_vdev_create;
	}

	status = wlan_objmgr_register_vdev_destroy_handler(
				WLAN_UMAC_COMP_POLICY_MGR,
				policy_mgr_vdev_obj_destroy_cb,
				NULL);
	if (status != QDF_STATUS_SUCCESS) {
		policy_mgr_err("Failed to register vdev obj delete cback");
		goto err_vdev_delete;
	}

	status = wlan_objmgr_register_vdev_status_handler(
				WLAN_UMAC_COMP_POLICY_MGR,
				policy_mgr_vdev_obj_status_cb,
				NULL);
	if (status != QDF_STATUS_SUCCESS) {
		policy_mgr_err("Failed to register vdev obj status cback");
		goto err_vdev_status;
	}

	policy_mgr_notice("Callbacks registered with obj mgr");

	return QDF_STATUS_SUCCESS;

err_vdev_status:
	wlan_objmgr_unregister_vdev_destroy_handler(WLAN_UMAC_COMP_POLICY_MGR,
						policy_mgr_vdev_obj_destroy_cb,
						NULL);
err_vdev_delete:
	wlan_objmgr_unregister_vdev_create_handler(WLAN_UMAC_COMP_POLICY_MGR,
						policy_mgr_vdev_obj_create_cb,
						NULL);
err_vdev_create:
	wlan_objmgr_unregister_pdev_destroy_handler(WLAN_UMAC_COMP_POLICY_MGR,
						policy_mgr_pdev_obj_destroy_cb,
						NULL);
err_pdev_delete:
	wlan_objmgr_unregister_pdev_create_handler(WLAN_UMAC_COMP_POLICY_MGR,
						policy_mgr_pdev_obj_create_cb,
						NULL);
err_pdev_create:
	wlan_objmgr_unregister_psoc_status_handler(WLAN_UMAC_COMP_POLICY_MGR,
						policy_mgr_psoc_obj_status_cb,
						NULL);
err_psoc_status:
	wlan_objmgr_unregister_psoc_destroy_handler(WLAN_UMAC_COMP_POLICY_MGR,
						policy_mgr_psoc_obj_destroy_cb,
						NULL);
err_psoc_delete:
	wlan_objmgr_unregister_psoc_create_handler(WLAN_UMAC_COMP_POLICY_MGR,
						policy_mgr_psoc_obj_create_cb,
						NULL);
err_psoc_create:
	return status;
}

QDF_STATUS policy_mgr_deinit(void)
{
	QDF_STATUS status;

	status = wlan_objmgr_unregister_psoc_status_handler(
				WLAN_UMAC_COMP_POLICY_MGR,
				policy_mgr_psoc_obj_status_cb,
				NULL);
	if (status != QDF_STATUS_SUCCESS)
		policy_mgr_err("Failed to deregister psoc obj status cback");

	status = wlan_objmgr_unregister_psoc_destroy_handler(
				WLAN_UMAC_COMP_POLICY_MGR,
				policy_mgr_psoc_obj_destroy_cb,
				NULL);
	if (status != QDF_STATUS_SUCCESS)
		policy_mgr_err("Failed to deregister psoc obj delete cback");

	status = wlan_objmgr_unregister_psoc_create_handler(
				WLAN_UMAC_COMP_POLICY_MGR,
				policy_mgr_psoc_obj_create_cb,
				NULL);
	if (status != QDF_STATUS_SUCCESS)
		policy_mgr_err("Failed to deregister psoc obj create cback");

	status = wlan_objmgr_unregister_pdev_destroy_handler(
				WLAN_UMAC_COMP_POLICY_MGR,
				policy_mgr_pdev_obj_destroy_cb,
				NULL);
	if (status != QDF_STATUS_SUCCESS)
		policy_mgr_err("Failed to deregister pdev obj delete cback");

	status = wlan_objmgr_unregister_pdev_create_handler(
				WLAN_UMAC_COMP_POLICY_MGR,
				policy_mgr_pdev_obj_create_cb,
				NULL);
	if (status != QDF_STATUS_SUCCESS)
		policy_mgr_err("Failed to deregister pdev obj create cback");

	status = wlan_objmgr_unregister_vdev_status_handler(
				WLAN_UMAC_COMP_POLICY_MGR,
				policy_mgr_vdev_obj_status_cb,
				NULL);
	if (status != QDF_STATUS_SUCCESS)
		policy_mgr_err("Failed to deregister vdev obj status cback");

	status = wlan_objmgr_unregister_vdev_destroy_handler(
				WLAN_UMAC_COMP_POLICY_MGR,
				policy_mgr_vdev_obj_destroy_cb,
				NULL);
	if (status != QDF_STATUS_SUCCESS)
		policy_mgr_err("Failed to deregister vdev obj delete cback");

	status = wlan_objmgr_unregister_vdev_create_handler(
				WLAN_UMAC_COMP_POLICY_MGR,
				policy_mgr_vdev_obj_create_cb,
				NULL);
	if (status != QDF_STATUS_SUCCESS)
		policy_mgr_err("Failed to deregister vdev obj create cback");

	policy_mgr_info("deregistered callbacks with obj mgr successfully");

	return status;
}

QDF_STATUS policy_mgr_psoc_open(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	if (!QDF_IS_STATUS_SUCCESS(qdf_mutex_create(
		&pm_ctx->qdf_conc_list_lock))) {
		policy_mgr_err("Failed to init qdf_conc_list_lock");
		QDF_ASSERT(0);
		return QDF_STATUS_E_FAILURE;
	}

	pm_ctx->sta_ap_intf_check_work_info = qdf_mem_malloc(
		sizeof(struct sta_ap_intf_check_work_ctx));
	if (!pm_ctx->sta_ap_intf_check_work_info) {
		qdf_mutex_destroy(&pm_ctx->qdf_conc_list_lock);
		return QDF_STATUS_E_FAILURE;
	}
	pm_ctx->sta_ap_intf_check_work_info->psoc = psoc;
	if (QDF_IS_STATUS_ERROR(qdf_delayed_work_create(
				&pm_ctx->sta_ap_intf_check_work,
				policy_mgr_check_sta_ap_concurrent_ch_intf,
				pm_ctx))) {
		policy_mgr_err("Failed to create dealyed work queue");
		qdf_mutex_destroy(&pm_ctx->qdf_conc_list_lock);
		qdf_mem_free(pm_ctx->sta_ap_intf_check_work_info);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_psoc_close(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	if (!QDF_IS_STATUS_SUCCESS(qdf_mutex_destroy(
		&pm_ctx->qdf_conc_list_lock))) {
		policy_mgr_err("Failed to destroy qdf_conc_list_lock");
		QDF_ASSERT(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (pm_ctx->hw_mode.hw_mode_list) {
		qdf_mem_free(pm_ctx->hw_mode.hw_mode_list);
		pm_ctx->hw_mode.hw_mode_list = NULL;
		policy_mgr_debug("HW list is freed");
	}

	if (pm_ctx->sta_ap_intf_check_work_info) {
		qdf_delayed_work_destroy(&pm_ctx->sta_ap_intf_check_work);
		qdf_mem_free(pm_ctx->sta_ap_intf_check_work_info);
		pm_ctx->sta_ap_intf_check_work_info = NULL;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * policy_mgr_update_5g_scc_prefer() - Update pcl if 5g scc is preferred
 * @psoc: psoc object
 *
 * Return: void
 */
static void policy_mgr_update_5g_scc_prefer(struct wlan_objmgr_psoc *psoc)
{
	enum policy_mgr_con_mode mode;

	for (mode = PM_STA_MODE; mode < PM_MAX_NUM_OF_MODE; mode++) {
		if (policy_mgr_get_5g_scc_prefer(psoc, mode)) {
			(*second_connection_pcl_dbs_table)
				[PM_STA_5_1x1][mode][PM_THROUGHPUT] =
					PM_SCC_CH_24G;
			policy_mgr_info("overwrite pm_second_connection_pcl_dbs_2x2_table, index %d mode %d system prefer %d new pcl %d",
					PM_STA_5_1x1, mode,
					PM_THROUGHPUT, PM_SCC_CH_24G);
			(*second_connection_pcl_dbs_table)
				[PM_STA_5_2x2][mode][PM_THROUGHPUT] =
					PM_SCC_CH_24G;
			policy_mgr_info("overwrite pm_second_connection_pcl_dbs_2x2_table, index %d mode %d system prefer %d new pcl %d",
					PM_STA_5_2x2, mode,
					PM_THROUGHPUT, PM_SCC_CH_24G);
		}
	}
}

#ifdef FEATURE_NO_DBS_INTRABAND_MCC_SUPPORT
static void policy_mgr_init_non_dbs_pcl(struct wlan_objmgr_psoc *psoc)
{
	struct wmi_unified *wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle) {
		policy_mgr_debug("Invalid WMI handle");
		return;
	}

	if (wmi_service_enabled(wmi_handle,
				wmi_service_no_interband_mcc_support) &&
	    !wmi_service_enabled(wmi_handle,
				wmi_service_dual_band_simultaneous_support)) {
		second_connection_pcl_non_dbs_table =
		&second_connection_pcl_nodbs_no_interband_mcc_table;
		third_connection_pcl_non_dbs_table =
		&third_connection_pcl_nodbs_no_interband_mcc_table;
	} else {
		second_connection_pcl_non_dbs_table =
		&second_connection_pcl_nodbs_table;
		third_connection_pcl_non_dbs_table =
		&third_connection_pcl_nodbs_table;
	}
}
#else
static void policy_mgr_init_non_dbs_pcl(struct wlan_objmgr_psoc *psoc)
{
	second_connection_pcl_non_dbs_table =
	&second_connection_pcl_nodbs_table;
	third_connection_pcl_non_dbs_table =
	&third_connection_pcl_nodbs_table;
}
#endif

QDF_STATUS policy_mgr_psoc_enable(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	bool enable_mcc_adaptive_sch = false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	policy_mgr_debug("Initializing the policy manager");

	/* init pm_conc_connection_list */
	qdf_mem_zero(pm_conc_connection_list, sizeof(pm_conc_connection_list));

	/* init dbs_opportunistic_timer */
	status = qdf_mc_timer_init(&pm_ctx->dbs_opportunistic_timer,
				QDF_TIMER_TYPE_SW,
				pm_dbs_opportunistic_timer_handler,
				(void *)psoc);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		policy_mgr_err("Failed to init DBS opportunistic timer");
		return status;
	}

	/* init connection_update_done_evt */
	status = policy_mgr_init_connection_update(pm_ctx);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		policy_mgr_err("connection_update_done_evt init failed");
		return status;
	}

	status = qdf_event_create(&pm_ctx->opportunistic_update_done_evt);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		policy_mgr_err("opportunistic_update_done_evt init failed");
		return status;
	}

	status = qdf_event_create(&pm_ctx->channel_switch_complete_evt);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		policy_mgr_err("channel_switch_complete_evt init failed");
		return status;
	}
	policy_mgr_get_mcc_adaptive_sch(psoc, &enable_mcc_adaptive_sch);
	policy_mgr_set_dynamic_mcc_adaptive_sch(psoc, enable_mcc_adaptive_sch);
	pm_ctx->hw_mode_change_in_progress = POLICY_MGR_HW_MODE_NOT_IN_PROGRESS;
	/* reset sap mandatory channels */
	status = policy_mgr_reset_sap_mandatory_channels(pm_ctx);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to reset mandatory channels");
		return status;
	}

	/* init PCL table & function pointers based on HW capability */
	if (policy_mgr_is_hw_dbs_2x2_capable(psoc) ||
	    policy_mgr_is_hw_dbs_required_for_band(psoc, HW_MODE_MAC_BAND_2G))
		policy_mgr_get_current_pref_hw_mode_ptr =
		policy_mgr_get_current_pref_hw_mode_dbs_2x2;
	else if (policy_mgr_is_2x2_1x1_dbs_capable(psoc))
		policy_mgr_get_current_pref_hw_mode_ptr =
		policy_mgr_get_current_pref_hw_mode_dual_dbs;
	else
		policy_mgr_get_current_pref_hw_mode_ptr =
		policy_mgr_get_current_pref_hw_mode_dbs_1x1;

	if (policy_mgr_is_hw_dbs_2x2_capable(psoc) ||
	    policy_mgr_is_hw_dbs_required_for_band(psoc,
						   HW_MODE_MAC_BAND_2G) ||
	    policy_mgr_is_2x2_1x1_dbs_capable(psoc)) {
		second_connection_pcl_dbs_table =
		&pm_second_connection_pcl_dbs_2x2_table;
		policy_mgr_update_5g_scc_prefer(psoc);
	} else {
		second_connection_pcl_dbs_table =
		&pm_second_connection_pcl_dbs_1x1_table;
	}

	if (policy_mgr_is_hw_dbs_2x2_capable(psoc) ||
	    policy_mgr_is_hw_dbs_required_for_band(psoc,
						   HW_MODE_MAC_BAND_2G) ||
	    policy_mgr_is_2x2_1x1_dbs_capable(psoc))
		third_connection_pcl_dbs_table =
		&pm_third_connection_pcl_dbs_2x2_table;
	else
		third_connection_pcl_dbs_table =
		&pm_third_connection_pcl_dbs_1x1_table;

	/* Initialize non-DBS pcl table pointer to particular table*/
	policy_mgr_init_non_dbs_pcl(psoc);

	if (policy_mgr_is_hw_dbs_2x2_capable(psoc) ||
	    policy_mgr_is_hw_dbs_required_for_band(psoc,
						   HW_MODE_MAC_BAND_2G)) {
		next_action_two_connection_table =
		&pm_next_action_two_connection_dbs_2x2_table;
	} else if (policy_mgr_is_2x2_1x1_dbs_capable(psoc)) {
		next_action_two_connection_table =
		&pm_next_action_two_connection_dbs_2x2_5g_1x1_2g_table;
		next_action_two_connection_2x2_2g_1x1_5g_table =
		&pm_next_action_two_connection_dbs_2x2_2g_1x1_5g_table;
	} else {
		next_action_two_connection_table =
		&pm_next_action_two_connection_dbs_1x1_table;
	}

	if (policy_mgr_is_hw_dbs_2x2_capable(psoc) ||
	    policy_mgr_is_hw_dbs_required_for_band(psoc,
						   HW_MODE_MAC_BAND_2G)) {
		next_action_three_connection_table =
		&pm_next_action_three_connection_dbs_2x2_table;
	} else if (policy_mgr_is_2x2_1x1_dbs_capable(psoc)) {
		next_action_three_connection_table =
		&pm_next_action_three_connection_dbs_2x2_5g_1x1_2g_table;
		next_action_three_connection_2x2_2g_1x1_5g_table =
		&pm_next_action_three_connection_dbs_2x2_2g_1x1_5g_table;
	} else {
		next_action_three_connection_table =
		&pm_next_action_three_connection_dbs_1x1_table;
	}
	policy_mgr_debug("is DBS Capable %d, is SBS Capable %d",
			 policy_mgr_is_hw_dbs_capable(psoc),
			 policy_mgr_is_hw_sbs_capable(psoc));
	policy_mgr_debug("is2x2 %d, 2g-on-dbs %d is2x2+1x1 %d, is2x2_5g+1x1_2g %d, is2x2_2g+1x1_5g %d",
			 policy_mgr_is_hw_dbs_2x2_capable(psoc),
			 policy_mgr_is_hw_dbs_required_for_band(
				psoc, HW_MODE_MAC_BAND_2G),
			 policy_mgr_is_2x2_1x1_dbs_capable(psoc),
			 policy_mgr_is_2x2_5G_1x1_2G_dbs_capable(psoc),
			 policy_mgr_is_2x2_2G_1x1_5G_dbs_capable(psoc));

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_psoc_disable(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	/* destroy connection_update_done_evt */
	if (!QDF_IS_STATUS_SUCCESS(qdf_event_destroy
		(&pm_ctx->connection_update_done_evt))) {
		policy_mgr_err("Failed to destroy connection_update_done_evt");
		status = QDF_STATUS_E_FAILURE;
		QDF_ASSERT(0);
	}

	/* destroy opportunistic_update_done_evt */
	if (!QDF_IS_STATUS_SUCCESS(qdf_event_destroy
		(&pm_ctx->opportunistic_update_done_evt))) {
		policy_mgr_err("Failed to destroy opportunistic_update_done_evt");
		status = QDF_STATUS_E_FAILURE;
		QDF_ASSERT(0);
	}
	/* destroy channel_switch_complete_evt */
	if (!QDF_IS_STATUS_SUCCESS(qdf_event_destroy
		(&pm_ctx->channel_switch_complete_evt))) {
		policy_mgr_err("Failed to destroy channel_switch_complete evt");
		status = QDF_STATUS_E_FAILURE;
		QDF_ASSERT(0);
	}

	/* deallocate dbs_opportunistic_timer */
	if (QDF_TIMER_STATE_RUNNING ==
			qdf_mc_timer_get_current_state(
				&pm_ctx->dbs_opportunistic_timer)) {
		qdf_mc_timer_stop(&pm_ctx->dbs_opportunistic_timer);
	}

	if (!QDF_IS_STATUS_SUCCESS(qdf_mc_timer_destroy(
			&pm_ctx->dbs_opportunistic_timer))) {
		policy_mgr_err("Cannot deallocate dbs opportunistic timer");
		status = QDF_STATUS_E_FAILURE;
		QDF_ASSERT(0);
	}

	/* reset sap mandatory channels */
	if (QDF_IS_STATUS_ERROR(
		policy_mgr_reset_sap_mandatory_channels(pm_ctx))) {
		policy_mgr_err("failed to reset sap mandatory channels");
		status = QDF_STATUS_E_FAILURE;
		QDF_ASSERT(0);
	}

	/* deinit pm_conc_connection_list */
	qdf_mem_zero(pm_conc_connection_list, sizeof(pm_conc_connection_list));

	return status;
}

QDF_STATUS policy_mgr_register_conc_cb(struct wlan_objmgr_psoc *psoc,
				struct policy_mgr_conc_cbacks *conc_cbacks)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	pm_ctx->conc_cbacks.connection_info_update =
					conc_cbacks->connection_info_update;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_register_sme_cb(struct wlan_objmgr_psoc *psoc,
		struct policy_mgr_sme_cbacks *sme_cbacks)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	pm_ctx->sme_cbacks.sme_get_nss_for_vdev =
		sme_cbacks->sme_get_nss_for_vdev;
	pm_ctx->sme_cbacks.sme_nss_update_request =
		sme_cbacks->sme_nss_update_request;
	pm_ctx->sme_cbacks.sme_pdev_set_hw_mode =
		sme_cbacks->sme_pdev_set_hw_mode;
	pm_ctx->sme_cbacks.sme_soc_set_dual_mac_config =
		sme_cbacks->sme_soc_set_dual_mac_config;
	pm_ctx->sme_cbacks.sme_change_mcc_beacon_interval =
		sme_cbacks->sme_change_mcc_beacon_interval;
	pm_ctx->sme_cbacks.sme_get_ap_channel_from_scan =
		sme_cbacks->sme_get_ap_channel_from_scan;
	pm_ctx->sme_cbacks.sme_scan_result_purge =
		sme_cbacks->sme_scan_result_purge;
	pm_ctx->sme_cbacks.sme_rso_start_cb =
		sme_cbacks->sme_rso_start_cb;
	pm_ctx->sme_cbacks.sme_rso_stop_cb =
		sme_cbacks->sme_rso_stop_cb;

	return QDF_STATUS_SUCCESS;
}

/**
 * policy_mgr_register_hdd_cb() - register HDD callbacks
 * @psoc: PSOC object information
 * @hdd_cbacks: function pointers from HDD
 *
 * API, allows HDD to register callbacks to be invoked by policy
 * mgr
 *
 * Return: SUCCESS,
 *         Failure (if registration fails)
 */
QDF_STATUS policy_mgr_register_hdd_cb(struct wlan_objmgr_psoc *psoc,
		struct policy_mgr_hdd_cbacks *hdd_cbacks)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	pm_ctx->hdd_cbacks.sap_restart_chan_switch_cb =
		hdd_cbacks->sap_restart_chan_switch_cb;
	pm_ctx->hdd_cbacks.wlan_hdd_get_channel_for_sap_restart =
		hdd_cbacks->wlan_hdd_get_channel_for_sap_restart;
	pm_ctx->hdd_cbacks.get_mode_for_non_connected_vdev =
		hdd_cbacks->get_mode_for_non_connected_vdev;
	pm_ctx->hdd_cbacks.hdd_get_device_mode =
		hdd_cbacks->hdd_get_device_mode;
	pm_ctx->hdd_cbacks.hdd_is_chan_switch_in_progress =
		hdd_cbacks->hdd_is_chan_switch_in_progress;
	pm_ctx->hdd_cbacks.hdd_is_cac_in_progress =
		hdd_cbacks->hdd_is_cac_in_progress;
	pm_ctx->hdd_cbacks.hdd_get_ap_6ghz_capable =
		hdd_cbacks->hdd_get_ap_6ghz_capable;
	pm_ctx->hdd_cbacks.wlan_hdd_indicate_active_ndp_cnt =
		hdd_cbacks->wlan_hdd_indicate_active_ndp_cnt;
	pm_ctx->hdd_cbacks.wlan_get_ap_prefer_conc_ch_params =
		hdd_cbacks->wlan_get_ap_prefer_conc_ch_params;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_deregister_hdd_cb(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	pm_ctx->hdd_cbacks.sap_restart_chan_switch_cb = NULL;
	pm_ctx->hdd_cbacks.wlan_hdd_get_channel_for_sap_restart = NULL;
	pm_ctx->hdd_cbacks.get_mode_for_non_connected_vdev = NULL;
	pm_ctx->hdd_cbacks.hdd_get_device_mode = NULL;
	pm_ctx->hdd_cbacks.hdd_is_chan_switch_in_progress = NULL;
	pm_ctx->hdd_cbacks.hdd_is_cac_in_progress = NULL;
	pm_ctx->hdd_cbacks.hdd_get_ap_6ghz_capable = NULL;
	pm_ctx->hdd_cbacks.wlan_get_ap_prefer_conc_ch_params = NULL;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_register_wma_cb(struct wlan_objmgr_psoc *psoc,
		struct policy_mgr_wma_cbacks *wma_cbacks)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	pm_ctx->wma_cbacks.wma_get_connection_info =
		wma_cbacks->wma_get_connection_info;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_register_cdp_cb(struct wlan_objmgr_psoc *psoc,
		struct policy_mgr_cdp_cbacks *cdp_cbacks)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	pm_ctx->cdp_cbacks.cdp_update_mac_id =
		cdp_cbacks->cdp_update_mac_id;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_register_dp_cb(struct wlan_objmgr_psoc *psoc,
		struct policy_mgr_dp_cbacks *dp_cbacks)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	pm_ctx->dp_cbacks.hdd_disable_rx_ol_in_concurrency =
		dp_cbacks->hdd_disable_rx_ol_in_concurrency;
	pm_ctx->dp_cbacks.hdd_set_rx_mode_rps_cb =
		dp_cbacks->hdd_set_rx_mode_rps_cb;
	pm_ctx->dp_cbacks.hdd_ipa_set_mcc_mode_cb =
		dp_cbacks->hdd_ipa_set_mcc_mode_cb;
	pm_ctx->dp_cbacks.hdd_v2_flow_pool_map =
		dp_cbacks->hdd_v2_flow_pool_map;
	pm_ctx->dp_cbacks.hdd_v2_flow_pool_unmap =
		dp_cbacks->hdd_v2_flow_pool_unmap;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_register_tdls_cb(struct wlan_objmgr_psoc *psoc,
		struct policy_mgr_tdls_cbacks *tdls_cbacks)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	pm_ctx->tdls_cbacks.tdls_notify_increment_session =
		tdls_cbacks->tdls_notify_increment_session;
	pm_ctx->tdls_cbacks.tdls_notify_decrement_session =
		tdls_cbacks->tdls_notify_decrement_session;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_register_mode_change_cb(struct wlan_objmgr_psoc *psoc,
	send_mode_change_event_cb mode_change_cb)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	pm_ctx->mode_change_cb = mode_change_cb;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_deregister_mode_change_cb(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	pm_ctx->mode_change_cb = NULL;

	return QDF_STATUS_SUCCESS;
}
