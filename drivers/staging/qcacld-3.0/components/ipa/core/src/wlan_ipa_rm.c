/*
 * Copyright (c) 2013-2019 The Linux Foundation. All rights reserved.
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

/* Include Files */
#include "qdf_delayed_work.h"
#include "wlan_ipa_core.h"
#include "wlan_ipa_main.h"
#include "cdp_txrx_ipa.h"
#include "host_diag_core_event.h"

QDF_STATUS wlan_ipa_set_perf_level(struct wlan_ipa_priv *ipa_ctx,
				    uint64_t tx_packets,
				    uint64_t rx_packets)
{
	int ret;
	uint32_t next_bw;
	uint64_t total_packets = tx_packets + rx_packets;

	if ((!wlan_ipa_is_enabled(ipa_ctx->config)) ||
		(!wlan_ipa_is_clk_scaling_enabled(ipa_ctx->config)))
		return 0;

	if (total_packets > (ipa_ctx->config->bus_bw_high / 2))
		next_bw = ipa_ctx->config->ipa_bw_high;
	else if (total_packets > (ipa_ctx->config->bus_bw_medium / 2))
		next_bw = ipa_ctx->config->ipa_bw_medium;
	else
		next_bw = ipa_ctx->config->ipa_bw_low;

	if (ipa_ctx->curr_cons_bw != next_bw) {
		ipa_debug("Requesting IPA perf curr: %d, next: %d",
			  ipa_ctx->curr_cons_bw, next_bw);
		ret = cdp_ipa_set_perf_level(ipa_ctx->dp_soc,
					     QDF_IPA_CLIENT_WLAN1_CONS,
					     next_bw);
		if (ret) {
			ipa_err("RM CONS set perf profile failed: %d", ret);

			return QDF_STATUS_E_FAILURE;
		}
		ret = cdp_ipa_set_perf_level(ipa_ctx->dp_soc,
					     QDF_IPA_CLIENT_WLAN1_PROD,
					     next_bw);
		if (ret) {
			ipa_err("RM PROD set perf profile failed: %d", ret);
			return QDF_STATUS_E_FAILURE;
		}
		ipa_ctx->curr_cons_bw = next_bw;
		ipa_ctx->stats.num_cons_perf_req++;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_ipa_init_perf_level(struct wlan_ipa_priv *ipa_ctx)
{
	int ret;

	/* Set lowest bandwidth to start with */
	if (wlan_ipa_is_clk_scaling_enabled(ipa_ctx->config))
		return wlan_ipa_set_perf_level(ipa_ctx, 0, 0);

	ipa_debug("IPA clk scaling disabled. Set perf level to maximum %d",
		  WLAN_IPA_MAX_BANDWIDTH);

	ret = cdp_ipa_set_perf_level(ipa_ctx->dp_soc,
				     QDF_IPA_CLIENT_WLAN1_CONS,
				     WLAN_IPA_MAX_BANDWIDTH);
	if (ret) {
		ipa_err("CONS set perf profile failed: %d", ret);
		return QDF_STATUS_E_FAILURE;
	}

	ret = cdp_ipa_set_perf_level(ipa_ctx->dp_soc,
				     QDF_IPA_CLIENT_WLAN1_PROD,
				     WLAN_IPA_MAX_BANDWIDTH);
	if (ret) {
		ipa_err("PROD set perf profile failed: %d", ret);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_METERING
void wlan_ipa_init_metering(struct wlan_ipa_priv *ipa_ctx)
{
	qdf_event_create(&ipa_ctx->ipa_uc_sharing_stats_comp);
	qdf_event_create(&ipa_ctx->ipa_uc_set_quota_comp);
}
#endif

#ifndef CONFIG_IPA_WDI_UNIFIED_API
/**
 * wlan_ipa_rm_cons_release() - WLAN consumer resource release handler
 *
 * Callback function registered with IPA that is called when IPA wants
 * to release the WLAN consumer resource
 *
 * Return: 0 if the request is granted, negative errno otherwise
 */
static int wlan_ipa_rm_cons_release(void)
{
	return 0;
}

/**
 * wlan_ipa_wdi_rm_request() - Request resource from IPA
 * @ipa_ctx: IPA context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_ipa_wdi_rm_request(struct wlan_ipa_priv *ipa_ctx)
{
	int ret;

	if (!wlan_ipa_is_rm_enabled(ipa_ctx->config))
		return QDF_STATUS_SUCCESS;

	qdf_spin_lock_bh(&ipa_ctx->rm_lock);

	switch (ipa_ctx->rm_state) {
	case WLAN_IPA_RM_GRANTED:
		qdf_spin_unlock_bh(&ipa_ctx->rm_lock);
		return QDF_STATUS_SUCCESS;
	case WLAN_IPA_RM_GRANT_PENDING:
		qdf_spin_unlock_bh(&ipa_ctx->rm_lock);
		return QDF_STATUS_E_PENDING;
	case WLAN_IPA_RM_RELEASED:
		ipa_ctx->rm_state = WLAN_IPA_RM_GRANT_PENDING;
		break;
	}

	qdf_spin_unlock_bh(&ipa_ctx->rm_lock);

	ret = qdf_ipa_rm_inactivity_timer_request_resource(
			QDF_IPA_RM_RESOURCE_WLAN_PROD);

	qdf_spin_lock_bh(&ipa_ctx->rm_lock);
	if (ret == 0) {
		ipa_ctx->rm_state = WLAN_IPA_RM_GRANTED;
		ipa_ctx->stats.num_rm_grant_imm++;
	}

	if (ipa_ctx->wake_lock_released) {
		qdf_wake_lock_acquire(&ipa_ctx->wake_lock,
				      WIFI_POWER_EVENT_WAKELOCK_IPA);
		ipa_ctx->wake_lock_released = false;
	}
	qdf_spin_unlock_bh(&ipa_ctx->rm_lock);

	qdf_delayed_work_stop_sync(&ipa_ctx->wake_lock_work);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_ipa_wdi_rm_try_release(struct wlan_ipa_priv *ipa_ctx)
{
	int ret;

	if (!wlan_ipa_is_rm_enabled(ipa_ctx->config))
		return QDF_STATUS_SUCCESS;

	if (qdf_atomic_read(&ipa_ctx->tx_ref_cnt))
		return QDF_STATUS_E_AGAIN;

	qdf_spin_lock_bh(&ipa_ctx->pm_lock);

	if (!qdf_nbuf_is_queue_empty(&ipa_ctx->pm_queue_head)) {
		qdf_spin_unlock_bh(&ipa_ctx->pm_lock);
		return QDF_STATUS_E_AGAIN;
	}
	qdf_spin_unlock_bh(&ipa_ctx->pm_lock);

	qdf_spin_lock_bh(&ipa_ctx->rm_lock);
	switch (ipa_ctx->rm_state) {
	case WLAN_IPA_RM_GRANTED:
		break;
	case WLAN_IPA_RM_GRANT_PENDING:
		qdf_spin_unlock_bh(&ipa_ctx->rm_lock);
		return QDF_STATUS_E_PENDING;
	case WLAN_IPA_RM_RELEASED:
		qdf_spin_unlock_bh(&ipa_ctx->rm_lock);
		return QDF_STATUS_SUCCESS;
	}

	/* IPA driver returns immediately so set the state here to avoid any
	 * race condition.
	 */
	ipa_ctx->rm_state = WLAN_IPA_RM_RELEASED;
	ipa_ctx->stats.num_rm_release++;
	qdf_spin_unlock_bh(&ipa_ctx->rm_lock);

	ret = qdf_ipa_rm_inactivity_timer_release_resource(
				QDF_IPA_RM_RESOURCE_WLAN_PROD);

	if (qdf_unlikely(ret != 0)) {
		qdf_spin_lock_bh(&ipa_ctx->rm_lock);
		ipa_ctx->rm_state = WLAN_IPA_RM_GRANTED;
		qdf_spin_unlock_bh(&ipa_ctx->rm_lock);
		QDF_ASSERT(0);
		ipa_warn("rm_inactivity_timer_release_resource ret fail");
	}

	/*
	 * If wake_lock is released immediately, kernel would try to suspend
	 * immediately as well, Just avoid ping-pong between suspend-resume
	 * while there is healthy amount of data transfer going on by
	 * releasing the wake_lock after some delay.
	 */
	qdf_delayed_work_start(&ipa_ctx->wake_lock_work,
			       WLAN_IPA_RX_INACTIVITY_MSEC_DELAY);

	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_ipa_uc_rm_notify_handler() - IPA uC resource notification handler
 * @ipa_ctx: IPA context
 * @event: IPA RM event
 *
 * Return: None
 */
static void
wlan_ipa_uc_rm_notify_handler(struct wlan_ipa_priv *ipa_ctx,
			      qdf_ipa_rm_event_t event)
{
	if (!wlan_ipa_is_rm_enabled(ipa_ctx->config))
		return;

	ipa_debug("event code %d", event);

	switch (event) {
	case QDF_IPA_RM_RESOURCE_GRANTED:
		/* Differed RM Granted */
		qdf_mutex_acquire(&ipa_ctx->ipa_lock);
		if ((!ipa_ctx->resource_unloading) &&
		    (!ipa_ctx->activated_fw_pipe)) {
			wlan_ipa_uc_enable_pipes(ipa_ctx);
			ipa_ctx->resource_loading = false;
		}
		qdf_mutex_release(&ipa_ctx->ipa_lock);
		break;

	case QDF_IPA_RM_RESOURCE_RELEASED:
		/* Differed RM Released */
		ipa_ctx->resource_unloading = false;
		break;

	default:
		ipa_err("invalid event code %d", event);
		break;
	}
}

/**
 * wlan_ipa_uc_rm_notify_defer() - Defer IPA uC notification
 * * @data: IPA context
 *
 * This function is called when a resource manager event is received
 * from firmware in interrupt context.  This function will defer the
 * handling to the OL RX thread
 *
 * Return: None
 */
static void wlan_ipa_uc_rm_notify_defer(void *data)
{
	struct wlan_ipa_priv *ipa_ctx = data;
	qdf_ipa_rm_event_t event;
	struct uc_rm_work_struct *uc_rm_work = &ipa_ctx->uc_rm_work;

	event = uc_rm_work->event;

	wlan_ipa_uc_rm_notify_handler(ipa_ctx, event);
}

/**
 * wlan_ipa_wake_lock_timer_func() - Wake lock work handler
 * @data: IPA context
 *
 * When IPA resources are released in wlan_ipa_wdi_rm_try_release() we do
 * not want to immediately release the wake lock since the system
 * would then potentially try to suspend when there is a healthy data
 * rate.  Deferred work is scheduled and this function handles the
 * work.  When this function is called, if the IPA resource is still
 * released then we release the wake lock.
 *
 * Return: None
 */
static void wlan_ipa_wake_lock_timer_func(void *data)
{
	struct wlan_ipa_priv *ipa_ctx = data;

	qdf_spin_lock_bh(&ipa_ctx->rm_lock);

	if (ipa_ctx->rm_state != WLAN_IPA_RM_RELEASED)
		goto end;

	ipa_ctx->wake_lock_released = true;
	qdf_wake_lock_release(&ipa_ctx->wake_lock,
			      WIFI_POWER_EVENT_WAKELOCK_IPA);

end:
	qdf_spin_unlock_bh(&ipa_ctx->rm_lock);
}

/**
 * wlan_ipa_rm_cons_request() - WLAN consumer resource request handler
 *
 * Callback function registered with IPA that is called when IPA wants
 * to access the WLAN consumer resource
 *
 * Return: 0 if the request is granted, negative errno otherwise
 */
static int wlan_ipa_rm_cons_request(void)
{
	struct wlan_ipa_priv *ipa_ctx;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	ipa_ctx = wlan_ipa_get_obj_context();

	if (ipa_ctx->resource_loading) {
		ipa_err("IPA resource loading in progress");
		ipa_ctx->pending_cons_req = true;
		status = QDF_STATUS_E_PENDING;
	} else if (ipa_ctx->resource_unloading) {
		ipa_err("IPA resource unloading in progress");
		ipa_ctx->pending_cons_req = true;
		status = QDF_STATUS_E_PERM;
	}

	return qdf_status_to_os_return(status);
}

/**
 * wlan_ipa_rm_notify() - IPA resource manager notifier callback
 * @user_data: user data registered with IPA
 * @event: the IPA resource manager event that occurred
 * @data: the data associated with the event
 *
 * Return: None
 */
static void wlan_ipa_rm_notify(void *user_data, qdf_ipa_rm_event_t event,
			       unsigned long data)
{
	struct wlan_ipa_priv *ipa_ctx = user_data;

	if (qdf_unlikely(!ipa_ctx))
		return;

	if (!wlan_ipa_is_rm_enabled(ipa_ctx->config))
		return;

	ipa_debug("Evt: %d", event);

	switch (event) {
	case QDF_IPA_RM_RESOURCE_GRANTED:
		if (wlan_ipa_uc_is_enabled(ipa_ctx->config)) {
			/* RM Notification comes with ISR context
			 * it should be serialized into work queue to avoid
			 * ISR sleep problem
			 */
			ipa_ctx->uc_rm_work.event = event;
			qdf_sched_work(0, &ipa_ctx->uc_rm_work.work);
			break;
		}
		qdf_spin_lock_bh(&ipa_ctx->rm_lock);
		ipa_ctx->rm_state = WLAN_IPA_RM_GRANTED;
		qdf_spin_unlock_bh(&ipa_ctx->rm_lock);
		ipa_ctx->stats.num_rm_grant++;
		break;

	case QDF_IPA_RM_RESOURCE_RELEASED:
		ipa_debug("RM Release");
		ipa_ctx->resource_unloading = false;
		break;

	default:
		ipa_err("Unknown RM Evt: %d", event);
		break;
	}
}

QDF_STATUS wlan_ipa_wdi_setup_rm(struct wlan_ipa_priv *ipa_ctx)
{
	qdf_ipa_rm_create_params_t create_params;
	QDF_STATUS status;
	int ret;

	if (!wlan_ipa_is_rm_enabled(ipa_ctx->config))
		return 0;

	qdf_create_work(0, &ipa_ctx->uc_rm_work.work,
			wlan_ipa_uc_rm_notify_defer, ipa_ctx);
	qdf_mem_zero(&create_params, sizeof(create_params));
	create_params.name = QDF_IPA_RM_RESOURCE_WLAN_PROD;
	create_params.reg_params.user_data = ipa_ctx;
	create_params.reg_params.notify_cb = wlan_ipa_rm_notify;
	create_params.floor_voltage = QDF_IPA_VOLTAGE_LEVEL;

	ret = qdf_ipa_rm_create_resource(&create_params);
	if (ret) {
		ipa_err("Create RM resource failed: %d", ret);
		goto setup_rm_fail;
	}

	qdf_mem_zero(&create_params, sizeof(create_params));
	create_params.name = QDF_IPA_RM_RESOURCE_WLAN_CONS;
	create_params.request_resource = wlan_ipa_rm_cons_request;
	create_params.release_resource = wlan_ipa_rm_cons_release;
	create_params.floor_voltage = QDF_IPA_VOLTAGE_LEVEL;

	ret = qdf_ipa_rm_create_resource(&create_params);
	if (ret) {
		ipa_err("Create RM CONS resource failed: %d", ret);
		goto delete_prod;
	}

	qdf_ipa_rm_add_dependency(QDF_IPA_RM_RESOURCE_WLAN_PROD,
				  QDF_IPA_RM_RESOURCE_APPS_CONS);

	ret = qdf_ipa_rm_inactivity_timer_init(QDF_IPA_RM_RESOURCE_WLAN_PROD,
					WLAN_IPA_RX_INACTIVITY_MSEC_DELAY);
	if (ret) {
		ipa_err("Timer init failed: %d", ret);
		goto timer_init_failed;
	}

	status = qdf_delayed_work_create(&ipa_ctx->wake_lock_work,
					 wlan_ipa_wake_lock_timer_func,
					 ipa_ctx);
	if (QDF_IS_STATUS_ERROR(status))
		goto timer_destroy;

	qdf_wake_lock_create(&ipa_ctx->wake_lock, "wlan_ipa");
	qdf_spinlock_create(&ipa_ctx->rm_lock);
	ipa_ctx->rm_state = WLAN_IPA_RM_RELEASED;
	ipa_ctx->wake_lock_released = true;
	qdf_atomic_set(&ipa_ctx->tx_ref_cnt, 0);

	return QDF_STATUS_SUCCESS;

timer_destroy:
	qdf_ipa_rm_inactivity_timer_destroy(QDF_IPA_RM_RESOURCE_WLAN_PROD);

timer_init_failed:
	qdf_ipa_rm_delete_resource(QDF_IPA_RM_RESOURCE_APPS_CONS);

delete_prod:
	qdf_ipa_rm_delete_resource(QDF_IPA_RM_RESOURCE_WLAN_PROD);

setup_rm_fail:
	return QDF_STATUS_E_FAILURE;
}

void wlan_ipa_wdi_destroy_rm(struct wlan_ipa_priv *ipa_ctx)
{
	int ret;

	if (!wlan_ipa_is_rm_enabled(ipa_ctx->config))
		return;

	qdf_wake_lock_destroy(&ipa_ctx->wake_lock);
	qdf_delayed_work_destroy(&ipa_ctx->wake_lock_work);
	qdf_cancel_work(&ipa_ctx->uc_rm_work.work);
	qdf_spinlock_destroy(&ipa_ctx->rm_lock);

	qdf_ipa_rm_inactivity_timer_destroy(QDF_IPA_RM_RESOURCE_WLAN_PROD);

	ret = qdf_ipa_rm_delete_resource(QDF_IPA_RM_RESOURCE_WLAN_CONS);
	if (ret)
		ipa_err("RM CONS resource delete failed %d", ret);

	ret = qdf_ipa_rm_delete_resource(QDF_IPA_RM_RESOURCE_WLAN_PROD);
	if (ret)
		ipa_err("RM PROD resource delete failed %d", ret);
}

bool wlan_ipa_is_rm_released(struct wlan_ipa_priv *ipa_ctx)
{
	qdf_spin_lock_bh(&ipa_ctx->rm_lock);

	if (ipa_ctx->rm_state != WLAN_IPA_RM_RELEASED) {
		qdf_spin_unlock_bh(&ipa_ctx->rm_lock);
		return false;
	}

	qdf_spin_unlock_bh(&ipa_ctx->rm_lock);

	return true;
}
#endif /* CONFIG_IPA_WDI_UNIFIED_API */
