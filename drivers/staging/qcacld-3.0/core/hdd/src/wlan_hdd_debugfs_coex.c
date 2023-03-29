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
 * DOC: wlan_hdd_debugfs_coex.c
 *
 * This file currently supports the following debugfs:
 * Get the following information coex information
 * COEX STATE
 * COEX DPWB STATE
 * COEX TDM STATE
 * COEX IDRX STATE
 * COEX ANTENNA SHARING STATE
 *
 * Example to read the COEX STATE logging:
 * sm6150:/ # cat /sys/kernel/debug/wlan/mws_coex_state
 */

#include <wlan_hdd_includes.h>
#include <wlan_osif_request_manager.h>
#include <wlan_hdd_debugfs_coex.h>
#include "wmi_unified.h"
#include "wmi_unified_param.h"
#include "osif_sync.h"

#define MWS_DEBUGFS_PERMS	(QDF_FILE_USR_READ |	\
				 QDF_FILE_GRP_READ |	\
				 QDF_FILE_OTH_READ)

/* wait time for mws coex info in milliseconds */
#define WLAN_WAIT_TIME_MWS_COEX_INFO 800

struct mws_coex_state_priv {
	struct mws_coex_state coex_state;
};

struct mws_coex_dpwb_state_priv {
	struct mws_coex_dpwb_state dpwb_state;
};

struct mws_coex_tdm_state_priv {
	struct mws_coex_tdm_state tdm_state;
};

struct mws_coex_idrx_state_priv {
	struct mws_coex_idrx_state idrx_state;
};

struct mws_antenna_sharing_info_priv {
	struct mws_antenna_sharing_info antenna_sharing;
};

static void hdd_debugfs_mws_coex_info_cb(void *coex_info_data, void *context,
					 wmi_mws_coex_cmd_id cmd_id)
{
	struct osif_request *request = NULL;
	struct mws_coex_state_priv *coex_priv;
	struct mws_coex_dpwb_state_priv *dpwb_priv;
	struct mws_coex_tdm_state_priv *tdm_priv;
	struct mws_coex_idrx_state_priv *idrx_priv;
	struct mws_antenna_sharing_info_priv *antenna_priv;

	if (!coex_info_data) {
		hdd_err("data is null");
		return;
	}

	request = osif_request_get(context);
	if (!request) {
		hdd_err("obselete request");
		return;
	}

	switch (cmd_id) {
	case WMI_MWS_COEX_STATE:
		coex_priv = osif_request_priv(request);
		qdf_mem_copy(&coex_priv->coex_state, coex_info_data,
			     sizeof(struct mws_coex_state));
		break;
	case WMI_MWS_COEX_DPWB_STATE:
		dpwb_priv = osif_request_priv(request);
		qdf_mem_copy(&dpwb_priv->dpwb_state, coex_info_data,
			     sizeof(struct mws_coex_dpwb_state));
		break;
	case WMI_MWS_COEX_TDM_STATE:
		tdm_priv = osif_request_priv(request);
		qdf_mem_copy(&tdm_priv->tdm_state, coex_info_data,
			     sizeof(struct mws_coex_tdm_state));
		break;
	case WMI_MWS_COEX_IDRX_STATE:
		idrx_priv = osif_request_priv(request);
		qdf_mem_copy(&idrx_priv->idrx_state, coex_info_data,
			     sizeof(struct mws_coex_idrx_state));
		break;
	case WMI_MWS_COEX_ANTENNA_SHARING_STATE:
		antenna_priv = osif_request_priv(request);
		qdf_mem_copy(&antenna_priv->antenna_sharing, coex_info_data,
			     sizeof(struct mws_antenna_sharing_info));
		break;
	}

	osif_request_complete(request);
	osif_request_put(request);
}

static QDF_STATUS __hdd_debugfs_mws_coex_state_read(struct hdd_context *hdd_ctx,
						    qdf_debugfs_file_t file)
{
	struct hdd_adapter *adapter;
	QDF_STATUS status;
	int rc;
	struct osif_request *request;
	struct mws_coex_state *coex_state;
	struct mws_coex_state_priv *priv;
	static const struct osif_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = WLAN_WAIT_TIME_MWS_COEX_INFO,
	};
	void *cookie;

	adapter = hdd_get_first_valid_adapter(hdd_ctx);
	if (!adapter) {
		hdd_err("Failed to get adapter");
		return QDF_STATUS_E_INVAL;
	}

	if (!test_bit(DEVICE_IFACE_OPENED, &adapter->event_flags)) {
		hdd_err("Interface is not enabled");
		return QDF_STATUS_E_INVAL;
	}

	if (file->index > 0)
		return QDF_STATUS_SUCCESS;

	request = osif_request_alloc(&params);
	if (!request) {
		hdd_err("Request allocation failure");
		return QDF_STATUS_E_NOMEM;
	}

	cookie = osif_request_cookie(request);

	status = sme_get_mws_coex_info(hdd_ctx->mac_handle,
				       adapter->vdev_id, WMI_MWS_COEX_STATE,
				       hdd_debugfs_mws_coex_info_cb, cookie);

	if (QDF_IS_STATUS_ERROR(status)) {
		rc = qdf_status_to_os_return(status);
		goto exit;
	}

	rc = osif_request_wait_for_response(request);
	if (rc) {
		qdf_debugfs_printf(file, "Timedout while retrieving MWS coex state");
		rc = -ETIMEDOUT;
		goto exit;
	}

	priv = osif_request_priv(request);
	coex_state = &priv->coex_state;

	qdf_debugfs_printf(file, "vdev_id = %u\n"
				 "coex_scheme_bitmap = %u\n"
				 "active_conflict_count = %u\n"
				 "potential_conflict_count = %u\n"
				 "chavd_group0_bitmap = %u\n"
				 "chavd_group1_bitmap = %u\n"
				 "chavd_group2_bitmap = %u\n"
				 "chavd_group3_bitmap = %u\n",
			   coex_state->vdev_id,
			   coex_state->coex_scheme_bitmap,
			   coex_state->active_conflict_count,
			   coex_state->potential_conflict_count,
			   coex_state->chavd_group0_bitmap,
			   coex_state->chavd_group1_bitmap,
			   coex_state->chavd_group2_bitmap,
			   coex_state->chavd_group3_bitmap);

exit:
	osif_request_put(request);
	return qdf_status_from_os_return(rc);
 }

static QDF_STATUS hdd_debugfs_mws_coex_state_read(qdf_debugfs_file_t file,
						  void *arg)
{
	struct osif_psoc_sync *psoc_sync;
	struct hdd_context *hdd_ctx = arg;
	int ret;
	QDF_STATUS status;

	ret = osif_psoc_sync_op_start(wiphy_dev(hdd_ctx->wiphy), &psoc_sync);
	if (ret)
		return qdf_status_from_os_return(ret);

	status = __hdd_debugfs_mws_coex_state_read(hdd_ctx, file);

	osif_psoc_sync_op_stop(psoc_sync);
	return qdf_status_from_os_return(ret);
}

static QDF_STATUS __hdd_debugfs_mws_coex_dpwb_read(struct hdd_context *hdd_ctx,
						   qdf_debugfs_file_t file)
 {
	struct hdd_adapter *adapter;
	QDF_STATUS status;
	int rc;
	struct osif_request *request;
	struct mws_coex_dpwb_state *dpwb_state;
	struct mws_coex_dpwb_state_priv *dpwb_priv;
	static const struct osif_request_params params = {
		.priv_size = sizeof(*dpwb_priv),
		.timeout_ms = WLAN_WAIT_TIME_MWS_COEX_INFO,
	};
	void *cookie;

	adapter = hdd_get_first_valid_adapter(hdd_ctx);
	if (!adapter) {
		hdd_err("Failed to get adapter");
		return QDF_STATUS_E_INVAL;
	}

	if (!test_bit(DEVICE_IFACE_OPENED, &adapter->event_flags)) {
		hdd_err("Interface is not enabled");
		return QDF_STATUS_E_INVAL;
	}

	if (file->index > 0)
		return QDF_STATUS_SUCCESS;

	request = osif_request_alloc(&params);
	if (!request) {
		hdd_err("Request allocation failure");
		return QDF_STATUS_E_NOMEM;
	}

	cookie = osif_request_cookie(request);

	status = sme_get_mws_coex_info(hdd_ctx->mac_handle,
				       adapter->vdev_id,
				       WMI_MWS_COEX_DPWB_STATE,
				       hdd_debugfs_mws_coex_info_cb, cookie);

	if (QDF_IS_STATUS_ERROR(status)) {
		rc = qdf_status_to_os_return(status);
		goto exit;
	}

	rc = osif_request_wait_for_response(request);
	if (rc) {
		qdf_debugfs_printf(file, "Timedout while retrieving MWS coex dpwb state");
		rc = -ETIMEDOUT;
		goto exit;
	}

	dpwb_priv = osif_request_priv(request);
	dpwb_state = &dpwb_priv->dpwb_state;
	qdf_debugfs_printf(file, "vdev_id = %u\n"
				 "current_dpwb_state = %d\n"
				 "pnp1_value = %d\n"
				 "lte_dutycycle = %d\n"
				 "sinr_wlan_on = %d\n"
				 "bler_count = %u\n"
				 "block_count = %u\n"
				 "wlan_rssi_level = %u\n"
				 "wlan_rssi = %d\n"
				 "is_tdm_running = %u\n",
			   dpwb_state->vdev_id,
			   dpwb_state->current_dpwb_state,
			   dpwb_state->pnp1_value,
			   dpwb_state->lte_dutycycle,
			   dpwb_state->sinr_wlan_on,
			   dpwb_state->sinr_wlan_off,
			   dpwb_state->bler_count,
			   dpwb_state->block_count,
			   dpwb_state->wlan_rssi_level,
			   dpwb_state->wlan_rssi,
			   dpwb_state->is_tdm_running);

exit:
	osif_request_put(request);
	return qdf_status_from_os_return(rc);
}

static QDF_STATUS hdd_debugfs_mws_coex_dpwb_read(qdf_debugfs_file_t file,
						 void *arg)
{
	struct osif_psoc_sync *psoc_sync;
	struct hdd_context *hdd_ctx = arg;
	int ret;
	QDF_STATUS status;

	ret = osif_psoc_sync_op_start(wiphy_dev(hdd_ctx->wiphy), &psoc_sync);
	if (ret)
		return qdf_status_from_os_return(ret);

	status = __hdd_debugfs_mws_coex_dpwb_read(hdd_ctx, file);

	osif_psoc_sync_op_stop(psoc_sync);
	return qdf_status_from_os_return(ret);
}

static QDF_STATUS __hdd_debugfs_mws_tdm_state_read(struct hdd_context *hdd_ctx,
						   qdf_debugfs_file_t file)
{
	struct hdd_adapter *adapter;
	QDF_STATUS status;
	int rc;
	struct mws_coex_tdm_state *tdm_state;
	struct mws_coex_tdm_state_priv *tdm_priv;
	struct osif_request *request;
	static const struct osif_request_params params = {
		.priv_size = sizeof(*tdm_priv),
		.timeout_ms = WLAN_WAIT_TIME_MWS_COEX_INFO,
	};
	void *cookie;

	adapter = hdd_get_first_valid_adapter(hdd_ctx);
	if (!adapter) {
		hdd_err("Failed to get adapter");
		return QDF_STATUS_E_INVAL;
	}

	if (!test_bit(DEVICE_IFACE_OPENED, &adapter->event_flags)) {
		hdd_err("Interface is not enabled");
		return QDF_STATUS_E_INVAL;
	}

	if (file->index > 0)
		return QDF_STATUS_SUCCESS;

	request = osif_request_alloc(&params);
	if (!request) {
		hdd_err("Request allocation failure");
		return QDF_STATUS_E_NOMEM;
	}

	cookie = osif_request_cookie(request);

	status = sme_get_mws_coex_info(hdd_ctx->mac_handle,
				       adapter->vdev_id, WMI_MWS_COEX_TDM_STATE,
				       hdd_debugfs_mws_coex_info_cb, cookie);

	if (QDF_IS_STATUS_ERROR(status)) {
		rc = qdf_status_to_os_return(status);
		goto exit;
	}

	rc = osif_request_wait_for_response(request);
	if (rc) {
		qdf_debugfs_printf(file, "Timedout while retrieving MWS coex tdm state");
		rc = -ETIMEDOUT;
		goto exit;
	}

	tdm_priv = osif_request_priv(request);
	tdm_state = &tdm_priv->tdm_state;

	qdf_debugfs_printf(file, "vdev_id = %u\n"
				 "tdm_policy_bitmap = %u\n"
				 "tdm_sf_bitmap = %u\n",
			   tdm_state->vdev_id,
			   tdm_state->tdm_policy_bitmap,
			   tdm_state->tdm_sf_bitmap);

exit:
	osif_request_put(request);
	return qdf_status_from_os_return(rc);
}

static QDF_STATUS hdd_debugfs_mws_tdm_state_read(qdf_debugfs_file_t file,
						 void *arg)
{
	struct osif_psoc_sync *psoc_sync;
	struct hdd_context *hdd_ctx = arg;
	int ret;
	QDF_STATUS status;

	ret = osif_psoc_sync_op_start(wiphy_dev(hdd_ctx->wiphy), &psoc_sync);
	if (ret)
		return qdf_status_from_os_return(ret);

	status = __hdd_debugfs_mws_tdm_state_read(hdd_ctx, file);

	osif_psoc_sync_op_stop(psoc_sync);
	return qdf_status_from_os_return(ret);
}

static QDF_STATUS __hdd_debugfs_mws_coex_idrx_read(struct hdd_context *hdd_ctx,
						   qdf_debugfs_file_t file)
{
	struct hdd_adapter *adapter;
	QDF_STATUS status;
	int rc;
	struct osif_request *request;
	struct mws_coex_idrx_state_priv *idrx_priv;
	struct mws_coex_idrx_state *idrx_state;
	static const struct osif_request_params params = {
		.priv_size = sizeof(*idrx_priv),
		.timeout_ms = WLAN_WAIT_TIME_MWS_COEX_INFO,
	};
	void *cookie;

	adapter = hdd_get_first_valid_adapter(hdd_ctx);
	if (!adapter) {
		hdd_err("Failed to get adapter");
		return QDF_STATUS_E_INVAL;
	}

	if (!test_bit(DEVICE_IFACE_OPENED, &adapter->event_flags)) {
		hdd_err("Interface is not enabled");
		return QDF_STATUS_E_INVAL;
	}

	if (file->index > 0)
		return QDF_STATUS_SUCCESS;

	request = osif_request_alloc(&params);
	if (!request) {
		hdd_err("Request allocation failure");
		return QDF_STATUS_E_NOMEM;
	}

	cookie = osif_request_cookie(request);

	status = sme_get_mws_coex_info(hdd_ctx->mac_handle,
				       adapter->vdev_id,
				       WMI_MWS_COEX_IDRX_STATE,
				       hdd_debugfs_mws_coex_info_cb, cookie);

	if (QDF_IS_STATUS_ERROR(status)) {
		rc = qdf_status_to_os_return(status);
		goto exit;
	}

	rc = osif_request_wait_for_response(request);
	if (rc) {
		qdf_debugfs_printf(file, "Timedout while retrieving MWS coex idrx state");
		rc = -ETIMEDOUT;
		goto exit;
	}

	idrx_priv = osif_request_priv(request);
	idrx_state = &idrx_priv->idrx_state;
	qdf_debugfs_printf(file, "vdev_id = %u\n"
				 "sub0_techid = %u\n"
				 "sub0_policy = %u\n"
				 "sub0_is_link_critical = %u\n"
				 "sub0_static_power = %u\n"
				 "sub0_rssi = %d\n"
				 "sub1_techid = %d\n"
				 "sub1_policy = %d\n"
				 "sub1_is_link_critical = %d\n"
				 "sub1_static_power = %u\n"
				 "sub1_rssi = %d\n",
			   idrx_state->vdev_id,
			   idrx_state->sub0_techid,
			   idrx_state->sub0_policy,
			   idrx_state->sub0_is_link_critical,
			   idrx_state->sub0_static_power,
			   idrx_state->sub0_rssi,
			   idrx_state->sub1_techid,
			   idrx_state->sub1_policy,
			   idrx_state->sub1_is_link_critical,
			   idrx_state->sub1_static_power,
			   idrx_state->sub1_rssi);

exit:
	osif_request_put(request);
	return qdf_status_from_os_return(rc);
}

static QDF_STATUS hdd_debugfs_mws_coex_idrx_read(qdf_debugfs_file_t file,
						 void *arg)
{
	struct osif_psoc_sync *psoc_sync;
	struct hdd_context *hdd_ctx = arg;
	int ret;
	QDF_STATUS status;

	ret = osif_psoc_sync_op_start(wiphy_dev(hdd_ctx->wiphy), &psoc_sync);
	if (ret)
		return qdf_status_from_os_return(ret);

	status = __hdd_debugfs_mws_coex_idrx_read(hdd_ctx, file);

	osif_psoc_sync_op_stop(psoc_sync);
	return qdf_status_from_os_return(ret);
}

static QDF_STATUS __hdd_debugfs_mws_antenna_sharing_read(struct hdd_context
							 *hdd_ctx,
							 qdf_debugfs_file_t
							 file)
{
	struct hdd_adapter *adapter;
	QDF_STATUS status;
	int rc;
	struct mws_antenna_sharing_info *antenna_sharing;
	struct mws_antenna_sharing_info_priv *antenna_priv;
	struct osif_request *request;
	static const struct osif_request_params params = {
		.priv_size = sizeof(*antenna_priv),
		.timeout_ms = WLAN_WAIT_TIME_MWS_COEX_INFO,
	};
	void *cookie;

	adapter = hdd_get_first_valid_adapter(hdd_ctx);
	if (!adapter) {
		hdd_err("Failed to get adapter");
		return QDF_STATUS_E_INVAL;
	}

	if (!test_bit(DEVICE_IFACE_OPENED, &adapter->event_flags)) {
		hdd_err("Interface is not enabled");
		return QDF_STATUS_E_INVAL;
	}

	if (file->index > 0)
		return QDF_STATUS_SUCCESS;

	request = osif_request_alloc(&params);
	if (!request) {
		hdd_err("Request allocation failure");
		return QDF_STATUS_E_NOMEM;
	}

	cookie = osif_request_cookie(request);

	status = sme_get_mws_coex_info(hdd_ctx->mac_handle,
				       adapter->vdev_id,
				       WMI_MWS_COEX_ANTENNA_SHARING_STATE,
				       hdd_debugfs_mws_coex_info_cb, cookie);

	if (QDF_IS_STATUS_ERROR(status)) {
		rc = qdf_status_to_os_return(status);
		goto exit;
	}

	rc = osif_request_wait_for_response(request);
	if (rc) {
		qdf_debugfs_printf(file, "Timedout while retrieving MWS coex antenna sharing state");
		rc = -ETIMEDOUT;
		goto exit;
	}

	antenna_priv = osif_request_priv(request);
	antenna_sharing = &antenna_priv->antenna_sharing;
	qdf_debugfs_printf(file, "vdev_id = %u\n"
				 "coex_flags = %u\n"
				 "coex_config = %u\n"
				 "tx_chain_mask = %u\n"
				 "rx_chain_mask = %u\n"
				 "rx_nss = %u\n"
				 "force_mrc = %u\n"
				 "rssi_type = %u\n"
				 "chain0_rssi = %d\n"
				 "chain1_rssi = %d\n"
				 "combined_rssi = %d\n"
				 "imbalance = %u\n"
				 "mrc_threshold = %d\n"
				 "grant_duration = %u\n",
			   antenna_sharing->vdev_id,
			   antenna_sharing->coex_flags,
			   antenna_sharing->coex_config,
			   antenna_sharing->tx_chain_mask,
			   antenna_sharing->rx_chain_mask,
			   antenna_sharing->rx_nss,
			   antenna_sharing->force_mrc,
			   antenna_sharing->rssi_type,
			   antenna_sharing->chain0_rssi,
			   antenna_sharing->chain1_rssi,
			   antenna_sharing->combined_rssi,
			   antenna_sharing->imbalance,
			   antenna_sharing->mrc_threshold,
			   antenna_sharing->grant_duration);

exit:
	osif_request_put(request);
	return qdf_status_from_os_return(rc);
}

static QDF_STATUS hdd_debugfs_mws_antenna_sharing_read(qdf_debugfs_file_t file,
						       void *arg)
{
	struct osif_psoc_sync *psoc_sync;
	struct hdd_context *hdd_ctx = arg;
	int ret;
	QDF_STATUS status;

	ret = osif_psoc_sync_op_start(wiphy_dev(hdd_ctx->wiphy), &psoc_sync);
	if (ret)
		return qdf_status_from_os_return(ret);

	status = __hdd_debugfs_mws_antenna_sharing_read(hdd_ctx, file);

	osif_psoc_sync_op_stop(psoc_sync);
	return qdf_status_from_os_return(ret);
}

static struct qdf_debugfs_fops hdd_mws_debugfs_coex_state_fops = {
	.show  = hdd_debugfs_mws_coex_state_read,
};

static struct qdf_debugfs_fops hdd_mws_debugfs_fops_coex_dpwb_fops = {
	.show  = hdd_debugfs_mws_coex_dpwb_read,
};

static struct qdf_debugfs_fops hdd_mws_debugfs_tdm_state_fpos = {
	.show  = hdd_debugfs_mws_tdm_state_read,
};

static struct qdf_debugfs_fops hdd_mws_debugfs_idrx_state_fpos = {
	.show  = hdd_debugfs_mws_coex_idrx_read,
};

static struct qdf_debugfs_fops hdd_mws_debugfs_antenna_sharing_fpos = {
	.show  = hdd_debugfs_mws_antenna_sharing_read,
};

void hdd_debugfs_mws_coex_info_init(struct hdd_context *hdd_ctx)
{
	hdd_mws_debugfs_coex_state_fops.priv = hdd_ctx;
	hdd_mws_debugfs_fops_coex_dpwb_fops.priv = hdd_ctx;
	hdd_mws_debugfs_tdm_state_fpos.priv = hdd_ctx;
	hdd_mws_debugfs_idrx_state_fpos.priv = hdd_ctx;
	hdd_mws_debugfs_antenna_sharing_fpos.priv = hdd_ctx;
	if (!qdf_debugfs_create_file("mws_coex_state", MWS_DEBUGFS_PERMS,
				     NULL, &hdd_mws_debugfs_coex_state_fops))
		hdd_err("Failed to create the mws coex state file");
	if (!qdf_debugfs_create_file("mws_coex_dpwb_state", MWS_DEBUGFS_PERMS,
				     NULL,
				     &hdd_mws_debugfs_fops_coex_dpwb_fops))
		hdd_err("Failed to create the mws coex dpwb file");
	if (!qdf_debugfs_create_file("mws_coex_tdm_state", MWS_DEBUGFS_PERMS,
				     NULL, &hdd_mws_debugfs_tdm_state_fpos))
		hdd_err("Failed to create the mws coex tdm file");
	if (!qdf_debugfs_create_file("mws_coex_idrx", MWS_DEBUGFS_PERMS,
				     NULL, &hdd_mws_debugfs_idrx_state_fpos))
		hdd_err("Failed to create the mws coex idrx file");
	if (!qdf_debugfs_create_file("mws_coex_antenna_sharing",
				     MWS_DEBUGFS_PERMS,
				     NULL,
				     &hdd_mws_debugfs_antenna_sharing_fpos))
		hdd_err("Failed to create the mws coex antenna sharing file");
}

void hdd_debugfs_mws_coex_info_deinit(struct hdd_context *hdd_ctx)
{
	/**
	 * Coex info dosent have a directory it is removed as part of qdf remove
	 */
}
