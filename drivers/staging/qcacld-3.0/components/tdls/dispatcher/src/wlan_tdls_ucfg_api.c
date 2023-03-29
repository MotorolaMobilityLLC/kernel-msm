/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_tdls_ucfg_api.c
 *
 * TDLS north bound interface definitions
 */

#include <wlan_tdls_ucfg_api.h>
#include <wlan_tdls_tgt_api.h>
#include "../../core/src/wlan_tdls_main.h"
#include "../../core/src/wlan_tdls_cmds_process.h"
#include "../../core/src/wlan_tdls_ct.h"
#include "../../core/src/wlan_tdls_mgmt.h"
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_cmn.h>
#include "wlan_policy_mgr_api.h"
#include "wlan_scan_ucfg_api.h"
#include "wlan_tdls_cfg.h"
#include "cfg_ucfg_api.h"
#include "wlan_tdls_api.h"

QDF_STATUS ucfg_tdls_init(void)
{
	QDF_STATUS status;

	tdls_notice("tdls module dispatcher init");
	status = wlan_objmgr_register_psoc_create_handler(WLAN_UMAC_COMP_TDLS,
		tdls_psoc_obj_create_notification, NULL);

	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("Failed to register psoc create handler for tdls");
		return status;
	}

	status = wlan_objmgr_register_psoc_destroy_handler(WLAN_UMAC_COMP_TDLS,
		tdls_psoc_obj_destroy_notification, NULL);

	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("Failed to register psoc delete handler for tdls");
		goto fail_delete_psoc;
	}

	status = wlan_objmgr_register_vdev_create_handler(WLAN_UMAC_COMP_TDLS,
		tdls_vdev_obj_create_notification, NULL);

	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("Failed to register vdev create handler for tdls");
		goto fail_create_vdev;
	}

	status = wlan_objmgr_register_vdev_destroy_handler(WLAN_UMAC_COMP_TDLS,
		tdls_vdev_obj_destroy_notification, NULL);

	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("Failed to register vdev create handler for tdls");
		goto fail_delete_vdev;
	}
	tdls_notice("tdls module dispatcher init done");

	return status;
fail_delete_vdev:
	wlan_objmgr_unregister_vdev_create_handler(WLAN_UMAC_COMP_TDLS,
		tdls_vdev_obj_create_notification, NULL);

fail_create_vdev:
	wlan_objmgr_unregister_psoc_destroy_handler(WLAN_UMAC_COMP_TDLS,
		tdls_psoc_obj_destroy_notification, NULL);

fail_delete_psoc:
	wlan_objmgr_unregister_psoc_create_handler(WLAN_UMAC_COMP_TDLS,
		tdls_psoc_obj_create_notification, NULL);

	return status;
}

QDF_STATUS ucfg_tdls_deinit(void)
{
	QDF_STATUS ret;

	tdls_notice("tdls module dispatcher deinit");
	ret = wlan_objmgr_unregister_psoc_create_handler(WLAN_UMAC_COMP_TDLS,
				tdls_psoc_obj_create_notification, NULL);
	if (QDF_IS_STATUS_ERROR(ret))
		tdls_err("Failed to unregister psoc create handler");

	ret = wlan_objmgr_unregister_psoc_destroy_handler(WLAN_UMAC_COMP_TDLS,
				tdls_psoc_obj_destroy_notification, NULL);
	if (QDF_IS_STATUS_ERROR(ret))
		tdls_err("Failed to unregister psoc delete handler");

	ret = wlan_objmgr_unregister_vdev_create_handler(WLAN_UMAC_COMP_TDLS,
				tdls_vdev_obj_create_notification, NULL);
	if (QDF_IS_STATUS_ERROR(ret))
		tdls_err("Failed to unregister vdev create handler");

	ret = wlan_objmgr_unregister_vdev_destroy_handler(WLAN_UMAC_COMP_TDLS,
				tdls_vdev_obj_destroy_notification, NULL);

	if (QDF_IS_STATUS_ERROR(ret))
		tdls_err("Failed to unregister vdev delete handler");

	return ret;
}

/**
 * tdls_update_feature_flag() - update tdls feature flag
 * @tdls_soc_obj:   pointer to tdls psoc object
 *
 * This function updates tdls feature flag
 */
static void
tdls_update_feature_flag(struct tdls_soc_priv_obj *tdls_soc_obj)
{
	tdls_soc_obj->tdls_configs.tdls_feature_flags =
		((tdls_soc_obj->tdls_configs.tdls_off_chan_enable ?
		  1 << TDLS_FEATURE_OFF_CHANNEL : 0) |
		 (tdls_soc_obj->tdls_configs.tdls_wmm_mode_enable ?
		  1 << TDLS_FEATURE_WMM : 0) |
		 (tdls_soc_obj->tdls_configs.tdls_buffer_sta_enable ?
		  1 << TDLS_FEATURE_BUFFER_STA : 0) |
		 (tdls_soc_obj->tdls_configs.tdls_sleep_sta_enable ?
		  1 << TDLS_FEATURE_SLEEP_STA : 0) |
		 (tdls_soc_obj->tdls_configs.tdls_scan_enable ?
		  1 << TDLS_FEATURE_SCAN : 0) |
		 (tdls_soc_obj->tdls_configs.tdls_support_enable ?
		  1 << TDLS_FEATURE_ENABLE : 0) |
		 (tdls_soc_obj->tdls_configs.tdls_implicit_trigger_enable ?
		  1 << TDLS_FEAUTRE_IMPLICIT_TRIGGER : 0) |
		 (tdls_soc_obj->tdls_configs.tdls_external_control &
		  TDLS_STRICT_EXTERNAL_CONTROL ?
		  1 << TDLS_FEATURE_EXTERNAL_CONTROL : 0) |
		 (tdls_soc_obj->tdls_configs.tdls_external_control &
		  TDLS_LIBERAL_EXTERNAL_CONTROL ?
		  1 << TDLS_FEATURE_LIBERAL_EXTERNAL_CONTROL : 0));
}

/**
 * tdls_object_init_params() - init parameters for tdls object
 * @tdls_soc_obj: pointer to tdls psoc object
 *
 * This function init parameters for tdls object
 */
static QDF_STATUS tdls_object_init_params(
	struct tdls_soc_priv_obj *tdls_soc_obj)
{
	struct wlan_objmgr_psoc *psoc;

	if (!tdls_soc_obj) {
		tdls_err("invalid param");
		return QDF_STATUS_E_INVAL;
	}

	psoc = tdls_soc_obj->soc;
	if (!psoc) {
		tdls_err("invalid psoc object");
		return QDF_STATUS_E_INVAL;
	}

	tdls_soc_obj->tdls_configs.tdls_tx_states_period =
			cfg_get(psoc, CFG_TDLS_TX_STATS_PERIOD);
	tdls_soc_obj->tdls_configs.tdls_tx_pkt_threshold =
			cfg_get(psoc, CFG_TDLS_TX_PACKET_THRESHOLD);
	tdls_soc_obj->tdls_configs.tdls_rx_pkt_threshold =
			cfg_get(psoc, CFG_TDLS_RX_FRAME_THRESHOLD);
	tdls_soc_obj->tdls_configs.tdls_max_discovery_attempt =
			cfg_get(psoc, CFG_TDLS_MAX_DISCOVERY_ATTEMPT);
	tdls_soc_obj->tdls_configs.tdls_idle_timeout =
			cfg_get(psoc, CFG_TDLS_IDLE_TIMEOUT);
	tdls_soc_obj->tdls_configs.tdls_idle_pkt_threshold =
			cfg_get(psoc, CFG_TDLS_IDLE_PACKET_THRESHOLD);
	tdls_soc_obj->tdls_configs.tdls_rssi_trigger_threshold =
			cfg_get(psoc, CFG_TDLS_RSSI_TRIGGER_THRESHOLD);
	tdls_soc_obj->tdls_configs.tdls_rssi_teardown_threshold =
			cfg_get(psoc, CFG_TDLS_RSSI_TEARDOWN_THRESHOLD);
	tdls_soc_obj->tdls_configs.tdls_rssi_delta =
			cfg_get(psoc, CFG_TDLS_RSSI_DELTA);
	tdls_soc_obj->tdls_configs.tdls_uapsd_mask =
			cfg_get(psoc, CFG_TDLS_QOS_WMM_UAPSD_MASK);
	tdls_soc_obj->tdls_configs.tdls_uapsd_inactivity_time =
			cfg_get(psoc, CFG_TDLS_PUAPSD_INACT_TIME);
	tdls_soc_obj->tdls_configs.tdls_uapsd_pti_window =
			cfg_get(psoc, CFG_TDLS_PUAPSD_PEER_TRAFFIC_IND_WINDOW);
	tdls_soc_obj->tdls_configs.tdls_uapsd_ptr_timeout =
			cfg_get(psoc, CFG_TDLS_PUAPSD_PEER_TRAFFIC_RSP_TIMEOUT);
	tdls_soc_obj->tdls_configs.tdls_pre_off_chan_num =
			cfg_get(psoc, CFG_TDLS_PREFERRED_OFF_CHANNEL_NUM);
	tdls_soc_obj->tdls_configs.tdls_pre_off_chan_bw =
			cfg_get(psoc, CFG_TDLS_PREFERRED_OFF_CHANNEL_BW);
	tdls_soc_obj->tdls_configs.tdls_peer_kickout_threshold =
			cfg_get(psoc, CFG_TDLS_PEER_KICKOUT_THRESHOLD);
	tdls_soc_obj->tdls_configs.tdls_discovery_wake_timeout =
			cfg_get(psoc, CFG_TDLS_DISCOVERY_WAKE_TIMEOUT);
	tdls_soc_obj->tdls_configs.delayed_trig_framint =
			cfg_get(psoc, CFG_TL_DELAYED_TRGR_FRM_INTERVAL);
	tdls_soc_obj->tdls_configs.tdls_wmm_mode_enable =
			cfg_get(psoc,  CFG_TDLS_WMM_MODE_ENABLE);
	tdls_soc_obj->tdls_configs.tdls_off_chan_enable =
			cfg_get(psoc, CFG_TDLS_OFF_CHANNEL_ENABLED);
	tdls_soc_obj->tdls_configs.tdls_buffer_sta_enable =
			cfg_get(psoc, CFG_TDLS_BUF_STA_ENABLED);
	tdls_soc_obj->tdls_configs.tdls_scan_enable =
			cfg_get(psoc, CFG_TDLS_SCAN_ENABLE);
	tdls_soc_obj->tdls_configs.tdls_support_enable =
			cfg_get(psoc, CFG_TDLS_SUPPORT_ENABLE);
	tdls_soc_obj->tdls_configs.tdls_implicit_trigger_enable =
			cfg_get(psoc, CFG_TDLS_IMPLICIT_TRIGGER);
	tdls_soc_obj->tdls_configs.tdls_external_control =
			cfg_get(psoc, CFG_TDLS_EXTERNAL_CONTROL);
	tdls_soc_obj->max_num_tdls_sta =
			cfg_get(psoc, CFG_TDLS_MAX_PEER_COUNT);

	tdls_update_feature_flag(tdls_soc_obj);

	return QDF_STATUS_SUCCESS;
}

#ifdef TDLS_WOW_ENABLED
/**
 * tdls_wow_init(): Create/init wake lock for TDLS
 *
 * Create/init wake lock for TDLS if DVR isn't supported
 *
 * Return None
 */
static void tdls_wow_init(struct tdls_soc_priv_obj *soc_obj)
{
	soc_obj->is_prevent_suspend = false;
	soc_obj->is_drv_supported = qdf_is_drv_supported();
	if (!soc_obj->is_drv_supported) {
		qdf_wake_lock_create(&soc_obj->wake_lock, "wlan_tdls");
		qdf_runtime_lock_init(&soc_obj->runtime_lock);
	}
}

/**
 * tdls_wow_deinit(): Destroy/deinit wake lock for TDLS
 *
 * Destroy/deinit wake lock for TDLS if DVR isn't supported
 *
 * Return None
 */
static void tdls_wow_deinit(struct tdls_soc_priv_obj *soc_obj)
{
	if (!soc_obj->is_drv_supported) {
		qdf_runtime_lock_deinit(&soc_obj->runtime_lock);
		qdf_wake_lock_destroy(&soc_obj->wake_lock);
	}
}
#else
static void tdls_wow_init(struct tdls_soc_priv_obj *soc_obj)
{
}

static void tdls_wow_deinit(struct tdls_soc_priv_obj *soc_obj)
{
}
#endif

static QDF_STATUS tdls_global_init(struct tdls_soc_priv_obj *soc_obj)
{
	tdls_object_init_params(soc_obj);
	soc_obj->connected_peer_count = 0;
	soc_obj->tdls_nss_switch_in_progress = false;
	soc_obj->tdls_teardown_peers_cnt = 0;
	soc_obj->tdls_nss_teardown_complete = false;
	soc_obj->tdls_nss_transition_mode = TDLS_NSS_TRANSITION_S_UNKNOWN;
	soc_obj->enable_tdls_connection_tracker = false;
	soc_obj->tdls_external_peer_count = 0;
	soc_obj->tdls_disable_in_progress = false;

	qdf_spinlock_create(&soc_obj->tdls_ct_spinlock);
	tdls_wow_init(soc_obj);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS tdls_global_deinit(struct tdls_soc_priv_obj *soc_obj)
{
	tdls_wow_deinit(soc_obj);
	qdf_spinlock_destroy(&soc_obj->tdls_ct_spinlock);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_tdls_psoc_open(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;
	struct tdls_soc_priv_obj *soc_obj;

	tdls_debug("tdls psoc open");
	soc_obj = wlan_objmgr_psoc_get_comp_private_obj(psoc,
							WLAN_UMAC_COMP_TDLS);
	if (!soc_obj) {
		tdls_err("Failed to get tdls psoc component");
		return QDF_STATUS_E_FAILURE;
	}

	status = tdls_global_init(soc_obj);

	return status;
}

#ifdef WLAN_FEATURE_11AX
void ucfg_tdls_update_fw_11ax_capability(struct wlan_objmgr_psoc *psoc,
					 bool is_fw_tdls_11ax_capable)
{
	struct tdls_soc_priv_obj *soc_obj;

	soc_obj = wlan_objmgr_psoc_get_comp_private_obj(psoc,
							WLAN_UMAC_COMP_TDLS);
	if (!soc_obj) {
		tdls_err("Failed to get tdls psoc component");
		return;
	}

	soc_obj->fw_tdls_11ax_capability = is_fw_tdls_11ax_capable;
}

bool  ucfg_tdls_is_fw_11ax_capable(struct wlan_objmgr_psoc *psoc)
{
	struct tdls_soc_priv_obj *soc_obj;

	soc_obj = wlan_objmgr_psoc_get_comp_private_obj(psoc,
							WLAN_UMAC_COMP_TDLS);
	if (!soc_obj) {
		tdls_err("Failed to get tdls psoc component");
		return false;
	}
	tdls_debug("FW 11AX capability %d", soc_obj->fw_tdls_11ax_capability);

	return soc_obj->fw_tdls_11ax_capability;
}
#endif

QDF_STATUS ucfg_tdls_update_config(struct wlan_objmgr_psoc *psoc,
				   struct tdls_start_params *req)
{
	struct tdls_soc_priv_obj *soc_obj;
	uint32_t tdls_feature_flags;
	struct policy_mgr_tdls_cbacks tdls_pm_call_backs;
	uint8_t sta_idx;

	tdls_debug("tdls update config ");
	if (!psoc || !req) {
		tdls_err("psoc: 0x%pK, req: 0x%pK", psoc, req);
		return QDF_STATUS_E_FAILURE;
	}

	soc_obj = wlan_objmgr_psoc_get_comp_private_obj(psoc,
							WLAN_UMAC_COMP_TDLS);
	if (!soc_obj) {
		tdls_err("Failed to get tdls psoc component");
		return QDF_STATUS_E_FAILURE;
	}

	soc_obj->tdls_rx_cb = req->tdls_rx_cb;
	soc_obj->tdls_rx_cb_data = req->tdls_rx_cb_data;

	soc_obj->tdls_wmm_cb = req->tdls_wmm_cb;
	soc_obj->tdls_wmm_cb_data = req->tdls_wmm_cb_data;

	soc_obj->tdls_event_cb = req->tdls_event_cb;
	soc_obj->tdls_evt_cb_data = req->tdls_evt_cb_data;

	/* Save callbacks to register/deregister TDLS sta with datapath */
	soc_obj->tdls_reg_peer = req->tdls_reg_peer;
	soc_obj->tdls_peer_context = req->tdls_peer_context;

	/* Save legacy PE/WMA commands in TDLS soc object */
	soc_obj->tdls_send_mgmt_req = req->tdls_send_mgmt_req;
	soc_obj->tdls_add_sta_req = req->tdls_add_sta_req;
	soc_obj->tdls_del_sta_req = req->tdls_del_sta_req;
	soc_obj->tdls_update_peer_state = req->tdls_update_peer_state;
	soc_obj->tdls_del_all_peers = req->tdls_del_all_peers;
	soc_obj->tdls_update_dp_vdev_flags = req->tdls_update_dp_vdev_flags;
	soc_obj->tdls_dp_vdev_update = req->tdls_dp_vdev_update;
	soc_obj->tdls_osif_init_cb = req->tdls_osif_init_cb;
	soc_obj->tdls_osif_deinit_cb = req->tdls_osif_deinit_cb;
	tdls_pm_call_backs.tdls_notify_increment_session =
			tdls_notify_increment_session;

	tdls_pm_call_backs.tdls_notify_decrement_session =
			tdls_notify_decrement_session;
	if (QDF_STATUS_SUCCESS != policy_mgr_register_tdls_cb(
		psoc, &tdls_pm_call_backs)) {
		tdls_err("policy manager callback registration failed ");
		return QDF_STATUS_E_FAILURE;
	}

	tdls_update_feature_flag(soc_obj);
	tdls_feature_flags = soc_obj->tdls_configs.tdls_feature_flags;

	if (!TDLS_IS_IMPLICIT_TRIG_ENABLED(tdls_feature_flags))
		soc_obj->tdls_current_mode = TDLS_SUPPORT_EXP_TRIG_ONLY;
	else if (TDLS_IS_EXTERNAL_CONTROL_ENABLED(tdls_feature_flags))
		soc_obj->tdls_current_mode = TDLS_SUPPORT_EXT_CONTROL;
	else
		soc_obj->tdls_current_mode = TDLS_SUPPORT_IMP_MODE;

	soc_obj->tdls_last_mode = soc_obj->tdls_current_mode;
	if (TDLS_IS_BUFFER_STA_ENABLED(tdls_feature_flags) ||
	    TDLS_IS_SLEEP_STA_ENABLED(tdls_feature_flags) ||
	    TDLS_IS_OFF_CHANNEL_ENABLED(tdls_feature_flags))
		soc_obj->max_num_tdls_sta =
			WLAN_TDLS_STA_P_UAPSD_OFFCHAN_MAX_NUM;

	for (sta_idx = 0; sta_idx < soc_obj->max_num_tdls_sta; sta_idx++) {
		soc_obj->tdls_conn_info[sta_idx].valid_entry = false;
		soc_obj->tdls_conn_info[sta_idx].index =
						INVALID_TDLS_PEER_INDEX;
		soc_obj->tdls_conn_info[sta_idx].session_id = 255;
		qdf_mem_zero(&soc_obj->tdls_conn_info[sta_idx].peer_mac,
			     QDF_MAC_ADDR_SIZE);
	}
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_tdls_psoc_enable(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;

	tdls_debug("psoc tdls enable: 0x%pK", psoc);
	if (!psoc) {
		tdls_err("NULL psoc");
		return QDF_STATUS_E_FAILURE;
	}

	status = tgt_tdls_register_ev_handler(psoc);

	if (status != QDF_STATUS_SUCCESS)
		return status;

	status = wlan_serialization_register_comp_info_cb(psoc,
					WLAN_UMAC_COMP_TDLS,
					WLAN_SER_CMD_SCAN,
					tdls_scan_serialization_comp_info_cb);
	if (QDF_STATUS_SUCCESS != status) {
		tdls_err("Serialize scan cmd register failed ");
		return status;
	}

	/* register callbacks with tx/rx mgmt */
	status = tdls_mgmt_rx_ops(psoc, true);
	if (status != QDF_STATUS_SUCCESS)
		tdls_err("Failed to register mgmt rx callback, status:%d",
			status);
	return status;
}

QDF_STATUS ucfg_tdls_psoc_disable(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;
	struct tdls_soc_priv_obj *soc_obj = NULL;

	tdls_debug("psoc tdls disable: 0x%pK", psoc);
	if (!psoc) {
		tdls_err("NULL psoc");
		return QDF_STATUS_E_FAILURE;
	}

	status = tgt_tdls_unregister_ev_handler(psoc);
	if (QDF_IS_STATUS_ERROR(status))
		tdls_err("Failed to unregister tdls event handler");

	status = tdls_mgmt_rx_ops(psoc, false);
	if (QDF_IS_STATUS_ERROR(status))
		tdls_err("Failed to unregister mgmt rx callback");

	soc_obj = wlan_objmgr_psoc_get_comp_private_obj(psoc,
							WLAN_UMAC_COMP_TDLS);
	if (!soc_obj) {
		tdls_err("Failed to get tdls psoc component");
		return QDF_STATUS_E_FAILURE;
	}

	soc_obj->tdls_event_cb = NULL;
	soc_obj->tdls_evt_cb_data = NULL;

	return status;
}

QDF_STATUS ucfg_tdls_psoc_close(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct tdls_soc_priv_obj *tdls_soc;

	tdls_debug("tdls psoc close");
	tdls_soc = wlan_objmgr_psoc_get_comp_private_obj(psoc,
							WLAN_UMAC_COMP_TDLS);
	if (!tdls_soc) {
		tdls_err("Failed to get tdls psoc component");
		return QDF_STATUS_E_FAILURE;
	}

	status = tdls_global_deinit(tdls_soc);

	return status;
}

static QDF_STATUS ucfg_tdls_post_msg_flush_cb(struct scheduler_msg *msg)
{
	void *ptr = msg->bodyptr;
	struct wlan_objmgr_vdev *vdev = NULL;

	switch (msg->type) {
	case TDLS_NOTIFY_RESET_ADAPTERS:
		ptr = NULL;
		break;
	case TDLS_NOTIFY_STA_CONNECTION:
	case TDLS_NOTIFY_STA_DISCONNECTION:
		vdev = ((struct tdls_sta_notify_params *)ptr)->vdev;
		break;
	case TDLS_CMD_SET_TDLS_MODE:
		vdev = ((struct tdls_set_mode_params *)ptr)->vdev;
		break;
	case TDLS_CMD_TX_ACTION:
	case TDLS_CMD_SET_RESPONDER:
		break;
	case TDLS_CMD_ADD_STA:
		vdev = ((struct tdls_add_peer_request *)ptr)->vdev;
		break;
	case TDLS_CMD_CHANGE_STA:
		vdev = ((struct tdls_update_peer_request *)ptr)->vdev;
		break;
	case TDLS_CMD_ENABLE_LINK:
	case TDLS_CMD_DISABLE_LINK:
	case TDLS_CMD_REMOVE_FORCE_PEER:
	case TDLS_CMD_CONFIG_FORCE_PEER:
		vdev = ((struct tdls_oper_request *)ptr)->vdev;
		break;
	case TDLS_CMD_SET_OFFCHANNEL:
		vdev = ((struct tdls_set_offchannel *)ptr)->vdev;
		break;
	case TDLS_CMD_SET_OFFCHANMODE:
		vdev = ((struct tdls_set_offchanmode *)ptr)->vdev;
		break;
	case TDLS_CMD_SET_SECOFFCHANOFFSET:
		vdev = ((struct tdls_set_secoffchanneloffset *)ptr)->vdev;
		break;
	}

	if (vdev)
		wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);

	if (ptr)
		qdf_mem_free(ptr);

	msg->bodyptr = NULL;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_tdls_add_peer(struct wlan_objmgr_vdev *vdev,
			      struct tdls_add_peer_params *add_peer_req)
{
	struct scheduler_msg msg = {0, };
	struct tdls_add_peer_request *req;
	QDF_STATUS status;

	if (!vdev || !add_peer_req) {
		tdls_err("vdev: %pK, req %pK", vdev, add_peer_req);
		return QDF_STATUS_E_NULL_VALUE;
	}
	tdls_debug("vdevid: %d, peertype: %d",
		   add_peer_req->vdev_id, add_peer_req->peer_type);

	status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_TDLS_NB_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("can't get vdev");
		return status;
	}

	req = qdf_mem_malloc(sizeof(*req));
	if (!req) {
		status = QDF_STATUS_E_NOMEM;
		goto dec_ref;
	}

	qdf_mem_copy(&req->add_peer_req, add_peer_req, sizeof(*add_peer_req));
	req->vdev = vdev;

	msg.bodyptr = req;
	msg.callback = tdls_process_cmd;
	msg.type = TDLS_CMD_ADD_STA;
	msg.flush_callback = ucfg_tdls_post_msg_flush_cb;
	status = scheduler_post_message(QDF_MODULE_ID_HDD,
					QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_OS_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("post add peer msg fail");
		qdf_mem_free(req);
		goto dec_ref;
	}

	return status;
dec_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);
	return status;
}

QDF_STATUS ucfg_tdls_update_peer(struct wlan_objmgr_vdev *vdev,
				 struct tdls_update_peer_params *update_peer)
{
	struct scheduler_msg msg = {0,};
	struct tdls_update_peer_request *req;
	QDF_STATUS status;

	if (!vdev || !update_peer) {
		tdls_err("vdev: %pK, update_peer: %pK", vdev, update_peer);
		return QDF_STATUS_E_NULL_VALUE;
	}

	tdls_debug("vdev_id: %d, peertype: %d",
		   update_peer->vdev_id, update_peer->peer_type);
	status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_TDLS_NB_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("can't get vdev");
		return status;
	}
	req = qdf_mem_malloc(sizeof(*req));
	if (!req) {
		status = QDF_STATUS_E_NOMEM;
		goto dec_ref;
	}
	qdf_mem_copy(&req->update_peer_req, update_peer, sizeof(*update_peer));
	req->vdev = vdev;

	msg.bodyptr = req;
	msg.callback = tdls_process_cmd;
	msg.type = TDLS_CMD_CHANGE_STA;
	msg.flush_callback = ucfg_tdls_post_msg_flush_cb;
	status = scheduler_post_message(QDF_MODULE_ID_HDD,
					QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_OS_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("post update peer msg fail");
		qdf_mem_free(req);
		goto dec_ref;
	}

	return status;
dec_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);
	return status;
}

static char *tdls_get_oper_str(enum tdls_command_type cmd_type)
{
	switch (cmd_type) {
	case TDLS_CMD_ENABLE_LINK:
		return "Enable_TDLS_LINK";
	case TDLS_CMD_DISABLE_LINK:
		return "DISABLE_TDLS_LINK";
	case TDLS_CMD_REMOVE_FORCE_PEER:
		return "REMOVE_FORCE_PEER";
	case TDLS_CMD_CONFIG_FORCE_PEER:
		return "CONFIG_FORCE_PEER";
	default:
		return "ERR:UNKNOWN OPER";
	}
}

QDF_STATUS ucfg_tdls_oper(struct wlan_objmgr_vdev *vdev,
			  const uint8_t *macaddr, enum tdls_command_type cmd)
{
	struct scheduler_msg msg = {0,};
	struct tdls_oper_request *req;
	QDF_STATUS status;

	if (!vdev || !macaddr) {
		tdls_err("vdev: %pK, mac %pK", vdev, macaddr);
		return QDF_STATUS_E_NULL_VALUE;
	}

	tdls_debug("%s for peer " QDF_MAC_ADDR_FMT,
		   tdls_get_oper_str(cmd),
		   QDF_MAC_ADDR_REF(macaddr));

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return QDF_STATUS_E_NOMEM;

	status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_TDLS_NB_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("can't get vdev");
		goto error;
	}

	qdf_mem_copy(req->peer_addr, macaddr, QDF_MAC_ADDR_SIZE);
	req->vdev = vdev;

	msg.bodyptr = req;
	msg.callback = tdls_process_cmd;
	msg.type = cmd;
	msg.flush_callback = ucfg_tdls_post_msg_flush_cb;
	status = scheduler_post_message(QDF_MODULE_ID_HDD,
					QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_OS_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("post msg for %s fail", tdls_get_oper_str(cmd));
		goto dec_ref;
	}

	return status;
dec_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);
error:
	qdf_mem_free(req);
	return status;
}

QDF_STATUS ucfg_tdls_get_all_peers(struct wlan_objmgr_vdev *vdev,
				   char *buf, int buflen)
{
	struct scheduler_msg msg = {0, };
	struct tdls_get_all_peers *tdls_peers;
	QDF_STATUS status;

	tdls_peers = qdf_mem_malloc(sizeof(*tdls_peers));
	if (!tdls_peers)
		return QDF_STATUS_E_NOMEM;

	tdls_peers->vdev = vdev;
	tdls_peers->buf_len = buflen;
	tdls_peers->buf = buf;

	msg.bodyptr = tdls_peers;
	msg.callback = tdls_process_cmd;
	msg.type = TDLS_CMD_GET_ALL_PEERS;
	status = scheduler_post_message(QDF_MODULE_ID_HDD,
					QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_OS_IF, &msg);

	if (status != QDF_STATUS_SUCCESS)
		qdf_mem_free(tdls_peers);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS tdls_send_mgmt_frame_flush_callback(struct scheduler_msg *msg)
{
	struct tdls_action_frame_request *req;

	if (!msg || !msg->bodyptr) {
		tdls_err("msg or msg->bodyptr is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}
	req = msg->bodyptr;
	if (req->vdev)
		wlan_objmgr_vdev_release_ref(req->vdev, WLAN_TDLS_NB_ID);

	qdf_mem_free(req);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_tdls_send_mgmt_frame(
				struct tdls_action_frame_request *req)
{
	struct scheduler_msg msg = {0, };
	struct tdls_action_frame_request *mgmt_req;
	QDF_STATUS status;

	if (!req || !req->vdev) {
		tdls_err("Invalid mgmt req params %pK", req);
		return QDF_STATUS_E_NULL_VALUE;
	}

	mgmt_req = qdf_mem_malloc(sizeof(*mgmt_req) +
					req->len);
	if (!mgmt_req)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_copy(mgmt_req, req, sizeof(*req));

	/*populate the additional IE's */
	if ((0 != req->len) && (req->cmd_buf)) {
		qdf_mem_copy(mgmt_req->tdls_mgmt.buf, req->cmd_buf,
				req->len);
		mgmt_req->tdls_mgmt.len = req->len;
	} else {
		mgmt_req->tdls_mgmt.len = 0;
	}

	tdls_debug("vdev id: %d, session id : %d", mgmt_req->vdev_id,
		    mgmt_req->session_id);
	status = wlan_objmgr_vdev_try_get_ref(req->vdev, WLAN_TDLS_NB_ID);

	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("Unable to get vdev reference for tdls module");
		goto mem_free;
	}

	msg.bodyptr = mgmt_req;
	msg.callback = tdls_process_cmd;
	msg.flush_callback = tdls_send_mgmt_frame_flush_callback;
	msg.type = TDLS_CMD_TX_ACTION;
	status = scheduler_post_message(QDF_MODULE_ID_HDD,
					QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_OS_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status))
		goto release_ref;

	return status;

release_ref:
	wlan_objmgr_vdev_release_ref(req->vdev, WLAN_TDLS_NB_ID);
mem_free:
	qdf_mem_free(mgmt_req);
	return status;
}

QDF_STATUS ucfg_tdls_responder(struct tdls_set_responder_req *req)
{
	struct scheduler_msg msg = {0, };
	struct tdls_set_responder_req *msg_req;
	QDF_STATUS status;

	if (!req || !req->vdev) {
		tdls_err("invalid input %pK", req);
		return QDF_STATUS_E_NULL_VALUE;
	}

	msg_req = qdf_mem_malloc(sizeof(*msg_req));
	if (!msg_req)
		return QDF_STATUS_E_NULL_VALUE;

	msg_req->responder = req->responder;
	msg_req->vdev = req->vdev;
	qdf_mem_copy(msg_req->peer_mac, req->peer_mac, QDF_MAC_ADDR_SIZE);

	msg.bodyptr = msg_req;
	msg.callback = tdls_process_cmd;
	msg.flush_callback = ucfg_tdls_post_msg_flush_cb;
	msg.type = TDLS_CMD_SET_RESPONDER;
	status = scheduler_post_message(QDF_MODULE_ID_HDD,
					QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_OS_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status))
		qdf_mem_free(msg_req);

	return status;
}

void ucfg_tdls_teardown_links_sync(struct wlan_objmgr_psoc *psoc)
{
	return wlan_tdls_teardown_links_sync(psoc);
}

QDF_STATUS ucfg_tdls_teardown_links(struct wlan_objmgr_psoc *psoc)
{
	return wlan_tdls_teardown_links(psoc);
}

QDF_STATUS ucfg_tdls_notify_reset_adapter(struct wlan_objmgr_vdev *vdev)
{
	QDF_STATUS status;
	struct scheduler_msg msg = {0, };

	if (!vdev) {
		tdls_err("vdev is NULL ");
		return QDF_STATUS_E_NULL_VALUE;
	}
	tdls_debug("Enter ");
	msg.bodyptr = vdev;
	msg.callback = tdls_process_cmd;
	msg.flush_callback = ucfg_tdls_post_msg_flush_cb;
	msg.type = TDLS_NOTIFY_RESET_ADAPTERS;
	status = scheduler_post_message(QDF_MODULE_ID_HDD,
					QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_OS_IF, &msg);
	return status;
}

QDF_STATUS ucfg_tdls_notify_sta_connect(
	struct tdls_sta_notify_params *notify_info)
{
	struct scheduler_msg msg = {0, };
	struct tdls_sta_notify_params *notify;
	QDF_STATUS status;

	if (!notify_info || !notify_info->vdev) {
		tdls_err("notify_info %pK", notify_info);
		return QDF_STATUS_E_NULL_VALUE;
	}
	tdls_debug("Enter ");

	notify = qdf_mem_malloc(sizeof(*notify));
	if (!notify) {
		wlan_objmgr_vdev_release_ref(notify_info->vdev,
					     WLAN_TDLS_NB_ID);
		return QDF_STATUS_E_NULL_VALUE;
	}

	*notify = *notify_info;

	msg.bodyptr = notify;
	msg.callback = tdls_process_cmd;
	msg.type = TDLS_NOTIFY_STA_CONNECTION;
	msg.flush_callback = ucfg_tdls_post_msg_flush_cb;
	status = scheduler_post_message(QDF_MODULE_ID_HDD,
					QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_TARGET_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		wlan_objmgr_vdev_release_ref(notify->vdev, WLAN_TDLS_NB_ID);
		qdf_mem_free(notify);
	}

	tdls_debug("Exit ");
	return status;
}

QDF_STATUS ucfg_tdls_notify_sta_disconnect(
			struct tdls_sta_notify_params *notify_info)
{
	struct scheduler_msg msg = {0, };
	struct tdls_sta_notify_params *notify;
	QDF_STATUS status;

	if (!notify_info || !notify_info->vdev) {
		tdls_err("notify_info %pK", notify_info);
		return QDF_STATUS_E_NULL_VALUE;
	}

	tdls_debug("Enter ");

	notify = qdf_mem_malloc(sizeof(*notify));
	if (!notify) {
		wlan_objmgr_vdev_release_ref(notify_info->vdev, WLAN_TDLS_NB_ID);
		return QDF_STATUS_E_NULL_VALUE;
	}

	*notify = *notify_info;

	msg.bodyptr = notify;
	msg.callback = tdls_process_cmd;
	msg.type = TDLS_NOTIFY_STA_DISCONNECTION;
	msg.flush_callback = ucfg_tdls_post_msg_flush_cb;
	status = scheduler_post_message(QDF_MODULE_ID_HDD,
					QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_TARGET_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		wlan_objmgr_vdev_release_ref(notify->vdev, WLAN_TDLS_NB_ID);
		qdf_mem_free(notify);
	}

	tdls_debug("Exit ");

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_tdls_set_operating_mode(
			struct tdls_set_mode_params *set_mode_params)
{
	struct scheduler_msg msg = {0, };
	struct tdls_set_mode_params *set_mode;
	QDF_STATUS status;

	if (!set_mode_params || !set_mode_params->vdev) {
		tdls_err("set_mode_params %pK", set_mode_params);
		return QDF_STATUS_E_NULL_VALUE;
	}

	tdls_debug("Enter ");

	set_mode = qdf_mem_malloc(sizeof(*set_mode));
	if (!set_mode)
		return QDF_STATUS_E_NULL_VALUE;

	status = wlan_objmgr_vdev_try_get_ref(set_mode->vdev, WLAN_TDLS_NB_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("failed to get vdev ref");
		qdf_mem_free(set_mode);
		return status;
	}

	set_mode->source = set_mode_params->source;
	set_mode->tdls_mode = set_mode_params->tdls_mode;
	set_mode->update_last = set_mode_params->update_last;
	set_mode->vdev = set_mode_params->vdev;

	msg.bodyptr = set_mode;
	msg.callback = tdls_process_cmd;
	msg.type = TDLS_CMD_SET_TDLS_MODE;
	msg.flush_callback = ucfg_tdls_post_msg_flush_cb;
	status = scheduler_post_message(QDF_MODULE_ID_HDD,
					QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_OS_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		wlan_objmgr_vdev_release_ref(set_mode->vdev, WLAN_TDLS_NB_ID);
		qdf_mem_free(set_mode);
	}

	tdls_debug("Exit ");

	return QDF_STATUS_SUCCESS;
}

void ucfg_tdls_update_rx_pkt_cnt(struct wlan_objmgr_vdev *vdev,
				 struct qdf_mac_addr *mac_addr,
				 struct qdf_mac_addr *dest_mac_addr)
{
	tdls_update_rx_pkt_cnt(vdev, mac_addr, dest_mac_addr);

}

void ucfg_tdls_update_tx_pkt_cnt(struct wlan_objmgr_vdev *vdev,
				 struct qdf_mac_addr *mac_addr)
{
	tdls_update_tx_pkt_cnt(vdev, mac_addr);
}

QDF_STATUS ucfg_tdls_antenna_switch(struct wlan_objmgr_vdev *vdev,
				    uint32_t mode)
{
	QDF_STATUS status;
	struct tdls_antenna_switch_request *req;
	struct scheduler_msg msg = {0, };

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return QDF_STATUS_E_NOMEM;

	status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_TDLS_NB_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("can't get vdev");
		goto error;
	}

	req->vdev = vdev;
	req->mode = mode;

	msg.bodyptr = req;
	msg.callback = tdls_process_cmd;
	msg.flush_callback = tdls_antenna_switch_flush_callback;
	msg.type = TDLS_CMD_ANTENNA_SWITCH;
	status = scheduler_post_message(QDF_MODULE_ID_HDD,
					QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_OS_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("post antenna switch msg fail");
		goto dec_ref;
	}

	return status;

dec_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);
error:
	qdf_mem_free(req);
	return status;
}

QDF_STATUS ucfg_set_tdls_offchannel(struct wlan_objmgr_vdev *vdev,
				    int offchannel)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct scheduler_msg msg = {0, };
	struct tdls_set_offchannel *req;

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return QDF_STATUS_E_NOMEM;

	status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_TDLS_NB_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("can't get vdev");
		goto free;
	}

	req->offchannel = offchannel;
	req->vdev = vdev;
	req->callback = wlan_tdls_offchan_parms_callback;
	msg.bodyptr = req;
	msg.callback = tdls_process_cmd;
	msg.type = TDLS_CMD_SET_OFFCHANNEL;
	msg.flush_callback = ucfg_tdls_post_msg_flush_cb;
	status = scheduler_post_message(QDF_MODULE_ID_HDD, QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_OS_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("post set tdls offchannel msg fail");
		goto dec_ref;
	}

	return status;

dec_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);

free:
	qdf_mem_free(req);
	return status;
}

QDF_STATUS ucfg_set_tdls_offchan_mode(struct wlan_objmgr_vdev *vdev,
				      int offchanmode)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct scheduler_msg msg = {0, };
	struct tdls_set_offchanmode *req;

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return QDF_STATUS_E_NOMEM;

	status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_TDLS_NB_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("can't get vdev");
		goto free;
	}

	req->offchan_mode = offchanmode;
	req->vdev = vdev;
	req->callback = wlan_tdls_offchan_parms_callback;
	msg.bodyptr = req;
	msg.callback = tdls_process_cmd;
	msg.type = TDLS_CMD_SET_OFFCHANMODE;
	msg.flush_callback = ucfg_tdls_post_msg_flush_cb;
	status = scheduler_post_message(QDF_MODULE_ID_HDD, QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_OS_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("post set offchanmode msg fail");
		goto dec_ref;
	}

	return status;

dec_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);

free:
	qdf_mem_free(req);
	return status;
}

QDF_STATUS ucfg_set_tdls_secoffchanneloffset(struct wlan_objmgr_vdev *vdev,
					     int offchanoffset)
{
	int status = QDF_STATUS_SUCCESS;
	struct scheduler_msg msg = {0, };
	struct tdls_set_secoffchanneloffset *req;

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return QDF_STATUS_E_NOMEM;

	status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_TDLS_NB_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("can't get vdev");
		goto free;
	}

	req->offchan_offset = offchanoffset;
	req->vdev = vdev;
	req->callback = wlan_tdls_offchan_parms_callback;
	msg.bodyptr = req;
	msg.callback = tdls_process_cmd;
	msg.type = TDLS_CMD_SET_SECOFFCHANOFFSET;
	msg.flush_callback = ucfg_tdls_post_msg_flush_cb;
	status = scheduler_post_message(QDF_MODULE_ID_HDD, QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_OS_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("post set secoffchan offset msg fail");
		goto dec_ref;
	}
	return status;

dec_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);

free:
	qdf_mem_free(req);
	return status;
}

QDF_STATUS ucfg_tdls_set_rssi(struct wlan_objmgr_vdev *vdev,
			      uint8_t *mac, int8_t rssi)
{
	return tdls_set_rssi(vdev, mac, rssi);
}

/**
 * wlan_tdls_notify_connect_failure() - This api is called if STA/P2P
 * connection fails on one iface and to enable/disable TDLS on the other
 * STA/P2P iface which is already connected.
 * @psoc: psoc object
 *
 * Return: void
 */
static inline
void  wlan_tdls_notify_connect_failure(struct wlan_objmgr_psoc *psoc)
{
	return tdls_notify_decrement_session(psoc);
}

void ucfg_tdls_notify_connect_failure(struct wlan_objmgr_psoc *psoc)
{
	return wlan_tdls_notify_connect_failure(psoc);
}

struct wlan_objmgr_vdev *ucfg_get_tdls_vdev(struct wlan_objmgr_psoc *psoc,
					    wlan_objmgr_ref_dbgid dbg_id)
{
	return tdls_get_vdev(psoc, dbg_id);
}
