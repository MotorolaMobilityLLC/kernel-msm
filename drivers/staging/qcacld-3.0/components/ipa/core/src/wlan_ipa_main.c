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
 * DOC: contains ipa component main function definitions
 */

#include "wlan_ipa_main.h"
#include "wlan_ipa_core.h"
#include "wlan_ipa_tgt_api.h"
#include "cfg_ucfg_api.h"

static struct wlan_ipa_config *g_ipa_config;
static bool g_ipa_hw_support;

bool ipa_check_hw_present(void)
{
	/* Check if ipa hw is enabled */
	if (qdf_ipa_uc_reg_rdyCB(NULL) != -EPERM) {
		g_ipa_hw_support = true;
		return true;
	} else {
		return false;
	}
}

QDF_STATUS ipa_config_mem_alloc(void)
{
	struct wlan_ipa_config *ipa_cfg;

	if (g_ipa_config)
		return QDF_STATUS_SUCCESS;

	ipa_cfg = qdf_mem_malloc(sizeof(*ipa_cfg));
	if (!ipa_cfg)
		return QDF_STATUS_E_NOMEM;

	g_ipa_config = ipa_cfg;

	return QDF_STATUS_SUCCESS;
}

void ipa_config_mem_free(void)
{
	if (!g_ipa_config) {
		ipa_err("IPA config already freed");
		return;
	}

	qdf_mem_free(g_ipa_config);
	g_ipa_config = NULL;
}

bool ipa_is_hw_support(void)
{
	return g_ipa_hw_support;
}

bool ipa_config_is_enabled(void)
{
	return g_ipa_config ? wlan_ipa_is_enabled(g_ipa_config) : 0;
}

bool ipa_config_is_uc_enabled(void)
{
	return g_ipa_config ? wlan_ipa_uc_is_enabled(g_ipa_config) : 0;
}

QDF_STATUS ipa_obj_setup(struct wlan_ipa_priv *ipa_ctx)
{
	return wlan_ipa_setup(ipa_ctx, g_ipa_config);
}

QDF_STATUS ipa_obj_cleanup(struct wlan_ipa_priv *ipa_ctx)
{
	return wlan_ipa_cleanup(ipa_ctx);
}

QDF_STATUS ipa_send_uc_offload_enable_disable(struct wlan_objmgr_pdev *pdev,
				struct ipa_uc_offload_control_params *req)
{
	return tgt_ipa_uc_offload_enable_disable(pdev, req);
}

void ipa_set_dp_handle(struct wlan_objmgr_psoc *psoc, void *dp_soc)
{
	struct wlan_objmgr_pdev *pdev;
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	pdev = wlan_objmgr_get_pdev_by_id(psoc, 0,
					  WLAN_IPA_ID);

	if (!pdev) {
		ipa_err("Failed to get pdev handle");
		return;
	}

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		wlan_objmgr_pdev_release_ref(pdev, WLAN_IPA_ID);
		return;
	}

	ipa_obj->dp_soc = dp_soc;
	wlan_objmgr_pdev_release_ref(pdev, WLAN_IPA_ID);
}

void ipa_set_pdev_id(struct wlan_objmgr_psoc *psoc, uint8_t pdev_id)
{
	struct wlan_objmgr_pdev *pdev;
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	pdev = wlan_objmgr_get_pdev_by_id(psoc, 0,
					  WLAN_IPA_ID);

	if (!pdev) {
		ipa_err("Failed to get pdev handle");
		return;
	}

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		wlan_objmgr_pdev_release_ref(pdev, WLAN_IPA_ID);
		return;
	}

	ipa_obj->dp_pdev_id = pdev_id;
	wlan_objmgr_pdev_release_ref(pdev, WLAN_IPA_ID);
}

QDF_STATUS ipa_rm_set_perf_level(struct wlan_objmgr_pdev *pdev,
				 uint64_t tx_packets, uint64_t rx_packets)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return QDF_STATUS_SUCCESS;
	}

	if (!ipa_is_ready())
		return QDF_STATUS_SUCCESS;

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return wlan_ipa_set_perf_level(ipa_obj, tx_packets, rx_packets);
}

void ipa_uc_info(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_is_ready())
		return;

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_uc_info(ipa_obj);
}

void ipa_uc_stat(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_is_ready())
		return;

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_uc_stat(ipa_obj);
}

void ipa_uc_rt_debug_host_dump(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_is_ready())
		return;

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_uc_rt_debug_host_dump(ipa_obj);
}

void ipa_dump_info(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_is_ready())
		return;

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_dump_info(ipa_obj);
}

void ipa_uc_stat_request(struct wlan_objmgr_pdev *pdev, uint8_t reason)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_is_ready())
		return;

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_uc_stat_request(ipa_obj, reason);
}

void ipa_uc_stat_query(struct wlan_objmgr_pdev *pdev,
		       uint32_t *ipa_tx_diff, uint32_t *ipa_rx_diff)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_is_ready())
		return;

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_uc_stat_query(ipa_obj, ipa_tx_diff, ipa_rx_diff);
}

void ipa_reg_sap_xmit_cb(struct wlan_objmgr_pdev *pdev, wlan_ipa_softap_xmit cb)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_is_ready())
		return;

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_reg_sap_xmit_cb(ipa_obj, cb);
}

void ipa_reg_send_to_nw_cb(struct wlan_objmgr_pdev *pdev,
			   wlan_ipa_send_to_nw cb)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_is_ready())
		return;

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_reg_send_to_nw_cb(ipa_obj, cb);
}

#ifdef IPA_LAN_RX_NAPI_SUPPORT
void ipa_reg_rps_enable_cb(struct wlan_objmgr_pdev *pdev,
			   wlan_ipa_rps_enable cb)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_is_ready())
		return;

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_reg_rps_enable_cb(ipa_obj, cb);
}
#endif

void ipa_set_mcc_mode(struct wlan_objmgr_pdev *pdev, bool mcc_mode)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_is_ready())
		return;

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_set_mcc_mode(ipa_obj, mcc_mode);
}

void ipa_set_dfs_cac_tx(struct wlan_objmgr_pdev *pdev, bool tx_block)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_is_ready())
		return;

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_set_dfs_cac_tx(ipa_obj, tx_block);
}

void ipa_set_ap_ibss_fwd(struct wlan_objmgr_pdev *pdev, uint8_t session_id,
			 bool intra_bss)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_is_ready())
		return;

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_set_ap_ibss_fwd(ipa_obj, session_id, intra_bss);
}

void ipa_uc_force_pipe_shutdown(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!pdev) {
		ipa_debug("objmgr pdev is null!");
		return;
	}

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_is_ready())
		return;

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	wlan_ipa_uc_disable_pipes(ipa_obj, true);
}

void ipa_flush(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_is_ready())
		return;

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_flush(ipa_obj);
}

QDF_STATUS ipa_suspend(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return QDF_STATUS_SUCCESS;
	}

	if (!ipa_is_ready())
		return QDF_STATUS_SUCCESS;

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return wlan_ipa_suspend(ipa_obj);
}

QDF_STATUS ipa_resume(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return QDF_STATUS_SUCCESS;
	}

	if (!ipa_is_ready())
		return QDF_STATUS_SUCCESS;

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return wlan_ipa_resume(ipa_obj);
}

QDF_STATUS ipa_uc_ol_init(struct wlan_objmgr_pdev *pdev,
			  qdf_device_t osdev)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return QDF_STATUS_SUCCESS;
	}

	if (!ipa_is_ready())
		return QDF_STATUS_SUCCESS;

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return wlan_ipa_uc_ol_init(ipa_obj, osdev);
}

bool ipa_is_tx_pending(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return QDF_STATUS_SUCCESS;
	}

	if (!ipa_is_ready())
		return QDF_STATUS_SUCCESS;

	ipa_obj = ipa_pdev_get_priv_obj(pdev);

	return wlan_ipa_is_tx_pending(ipa_obj);
}

QDF_STATUS ipa_uc_ol_deinit(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;
	QDF_STATUS status;

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return QDF_STATUS_SUCCESS;
	}

	ipa_init_deinit_lock();

	if (!ipa_is_ready()) {
		ipa_debug("ipa is not ready");
		status = QDF_STATUS_SUCCESS;
		goto out;
	}

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}

	status = wlan_ipa_uc_ol_deinit(ipa_obj);

out:
	ipa_init_deinit_unlock();
	return status;
}

QDF_STATUS ipa_send_mcc_scc_msg(struct wlan_objmgr_pdev *pdev,
				bool mcc_mode)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return QDF_STATUS_SUCCESS;
	}

	if (!ipa_is_ready())
		return QDF_STATUS_SUCCESS;

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return wlan_ipa_send_mcc_scc_msg(ipa_obj, mcc_mode);
}

QDF_STATUS ipa_wlan_evt(struct wlan_objmgr_pdev *pdev, qdf_netdev_t net_dev,
			uint8_t device_mode, uint8_t session_id,
			enum wlan_ipa_wlan_event ipa_event_type,
			uint8_t *mac_addr)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_is_ready())
		return QDF_STATUS_SUCCESS;

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return wlan_ipa_wlan_evt(net_dev, device_mode, session_id,
				 ipa_event_type, mac_addr);
}

int ipa_uc_smmu_map(bool map, uint32_t num_buf, qdf_mem_info_t *buf_arr)
{
	return wlan_ipa_uc_smmu_map(map, num_buf, buf_arr);
}

bool ipa_is_fw_wdi_activated(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_config_is_enabled()) {
		ipa_debug_rl("ipa is disabled");
		return false;
	}

	if (!ipa_is_ready()) {
		ipa_debug("ipa is not ready");
		return false;
	}

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err_rl("IPA object is NULL");
		return false;
	}

	return wlan_ipa_is_fw_wdi_activated(ipa_obj);
}

void ipa_uc_cleanup_sta(struct wlan_objmgr_pdev *pdev,
			qdf_netdev_t net_dev)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_is_ready()) {
		ipa_debug("ipa is not ready");
		return;
	}

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_uc_cleanup_sta(ipa_obj, net_dev);
}

QDF_STATUS ipa_uc_disconnect_ap(struct wlan_objmgr_pdev *pdev,
				qdf_netdev_t net_dev)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_is_ready())
		return QDF_STATUS_SUCCESS;

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return wlan_ipa_uc_disconnect_ap(ipa_obj, net_dev);
}

void ipa_cleanup_dev_iface(struct wlan_objmgr_pdev *pdev,
			   qdf_netdev_t net_dev)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_is_ready())
		return;

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_cleanup_dev_iface(ipa_obj, net_dev);
}

void ipa_uc_ssr_cleanup(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_is_ready())
		return;

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_uc_ssr_cleanup(ipa_obj);
}

void ipa_fw_rejuvenate_send_msg(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!pdev) {
		ipa_debug("objmgr pdev is null!");
		return;
	}

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_fw_rejuvenate_send_msg(ipa_obj);
}

void ipa_component_config_update(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;

	status = ipa_config_mem_alloc();
	if (QDF_IS_STATUS_ERROR(status)) {
		ipa_err("Failed to alloc g_ipa_config");
		return;
	}

	g_ipa_config->ipa_config =
		cfg_get(psoc, CFG_DP_IPA_OFFLOAD_CONFIG);
	g_ipa_config->desc_size =
		cfg_get(psoc, CFG_DP_IPA_DESC_SIZE);
	g_ipa_config->txbuf_count =
		qdf_rounddown_pow_of_two(cfg_get(psoc,
						 CFG_DP_IPA_UC_TX_BUF_COUNT));
	g_ipa_config->ipa_bw_high =
		cfg_get(psoc, CFG_DP_IPA_HIGH_BANDWIDTH_MBPS);
	g_ipa_config->ipa_bw_medium =
		cfg_get(psoc, CFG_DP_IPA_MEDIUM_BANDWIDTH_MBPS);
	g_ipa_config->ipa_bw_low =
		cfg_get(psoc, CFG_DP_IPA_LOW_BANDWIDTH_MBPS);
	g_ipa_config->bus_bw_high =
		cfg_get(psoc, CFG_DP_BUS_BANDWIDTH_HIGH_THRESHOLD);
	g_ipa_config->bus_bw_medium =
		cfg_get(psoc, CFG_DP_BUS_BANDWIDTH_MEDIUM_THRESHOLD);
	g_ipa_config->bus_bw_low =
		cfg_get(psoc, CFG_DP_BUS_BANDWIDTH_LOW_THRESHOLD);
	g_ipa_config->ipa_force_voting =
		cfg_get(psoc, CFG_DP_IPA_ENABLE_FORCE_VOTING);
}

void ipa_component_config_free(void)
{
	ipa_info("Free the IPA config memory");
	ipa_config_mem_free();
}

uint32_t ipa_get_tx_buf_count(void)
{
	return g_ipa_config ? g_ipa_config->txbuf_count : 0;
}

void ipa_update_tx_stats(struct wlan_objmgr_pdev *pdev, uint64_t sta_tx,
			 uint64_t ap_tx)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_config_is_enabled())
		return;

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	wlan_ipa_update_tx_stats(ipa_obj, sta_tx, ap_tx);
}

void ipa_flush_pending_vdev_events(struct wlan_objmgr_pdev *pdev,
				   uint8_t vdev_id)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_config_is_enabled())
		return;

	if (!ipa_is_ready())
		return;

	ipa_obj = ipa_pdev_get_priv_obj(pdev);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	wlan_ipa_flush_pending_vdev_events(ipa_obj, vdev_id);
}

