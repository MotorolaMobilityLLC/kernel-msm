/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: cds_api.c
 *
 * Connectivity driver services APIs
 */

#include <cds_api.h>
#include "sir_types.h"
#include "sir_api.h"
#include "sir_mac_prot_def.h"
#include "sme_api.h"
#include "mac_init_api.h"
#include "wlan_qct_sys.h"
#include "i_cds_packet.h"
#include "cds_reg_service.h"
#include "wma_types.h"
#include "wlan_hdd_main.h"
#include "wlan_hdd_power.h"
#include "wlan_hdd_tsf.h"
#include <linux/vmalloc.h>
#include <scheduler_core.h>

#include "pld_common.h"
#include "sap_api.h"
#include "bmi.h"
#include "ol_fw.h"
#include "ol_if_athvar.h"
#include "hif.h"
#include "wlan_policy_mgr_api.h"
#include "cds_utils.h"
#include "wlan_logging_sock_svc.h"
#include "wma.h"
#include "pktlog_ac.h"
#include "wlan_policy_mgr_api.h"

#include <cdp_txrx_cmn_reg.h>
#include <cdp_txrx_cfg.h>
#include <cdp_txrx_misc.h>
#include <ol_defines.h>
#include <dispatcher_init_deinit.h>
#include <cdp_txrx_handle.h>
#include <cdp_txrx_host_stats.h>
#include "target_type.h"
#include "wlan_ocb_ucfg_api.h"
#include "wlan_ipa_ucfg_api.h"
#include "dp_txrx.h"
#ifdef ENABLE_SMMU_S1_TRANSLATION
#include "pld_common.h"
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
#include <asm/dma-iommu.h>
#endif
#include <linux/iommu.h>
#endif

#ifdef QCA_WIFI_QCA8074
#include <target_if_dp.h>
#endif
#include "wlan_mlme_ucfg_api.h"
#include "cfg_ucfg_api.h"
#include "wlan_cp_stats_mc_ucfg_api.h"
#include <qdf_hang_event_notifier.h>
#include <qdf_notifier.h>
#include <qwlan_version.h>
#include <qdf_trace.h>

/* Preprocessor Definitions and Constants */

/* Preprocessor Definitions and Constants */

/* Data definitions */
static struct cds_context g_cds_context;
static struct cds_context *gp_cds_context;
static struct __qdf_device g_qdf_ctx;

static uint8_t cds_multicast_logging;

#define DRIVER_VER_LEN (11)
#define HANG_EVENT_VER_LEN (1)

struct cds_hang_event_fixed_param {
	uint16_t tlv_header;
	uint8_t recovery_reason;
	char driver_version[DRIVER_VER_LEN];
	char hang_event_version[HANG_EVENT_VER_LEN];
} qdf_packed;

#ifdef QCA_WIFI_QCA8074
static inline int
cds_send_delba(struct cdp_ctrl_objmgr_psoc *psoc,
	       uint8_t vdev_id, uint8_t *peer_macaddr,
	       uint8_t tid, uint8_t reason_code,
	       uint8_t cdp_reason_code)
{
	return wma_dp_send_delba_ind(vdev_id, peer_macaddr, tid,
				     reason_code, cdp_reason_code);
}

static struct ol_if_ops  dp_ol_if_ops = {
	.peer_set_default_routing = target_if_peer_set_default_routing,
	.peer_rx_reorder_queue_setup = target_if_peer_rx_reorder_queue_setup,
	.peer_rx_reorder_queue_remove = target_if_peer_rx_reorder_queue_remove,
	.is_hw_dbs_2x2_capable = policy_mgr_is_dp_hw_dbs_2x2_capable,
	.lro_hash_config = target_if_lro_hash_config,
	.rx_invalid_peer = wma_rx_invalid_peer_ind,
	.is_roam_inprogress = wma_is_roam_in_progress,
	.get_con_mode = cds_get_conparam,
	.send_delba = cds_send_delba,
	.dp_rx_get_pending = dp_rx_tm_get_pending,
#ifdef DP_MEM_PRE_ALLOC
	.dp_prealloc_get_context = dp_prealloc_get_context_memory,
	.dp_prealloc_put_context = dp_prealloc_put_context_memory,
	.dp_prealloc_get_consistent = dp_prealloc_get_coherent,
	.dp_prealloc_put_consistent = dp_prealloc_put_coherent,
	.dp_get_multi_pages = dp_prealloc_get_multi_pages,
	.dp_put_multi_pages = dp_prealloc_put_multi_pages
#endif
    /* TODO: Add any other control path calls required to OL_IF/WMA layer */
};
#else
static struct ol_if_ops  dp_ol_if_ops;
#endif

static void cds_trigger_recovery_work(void *param);

/**
 * struct cds_recovery_call_info - caller information for cds_trigger_recovery
 * @func: caller's function name
 * @line: caller's line number
 */
struct cds_recovery_call_info {
	const char *func;
	uint32_t line;
} __cds_recovery_caller;

/**
 * cds_recovery_work_init() - Initialize recovery work queue
 *
 * Return: none
 */
static QDF_STATUS cds_recovery_work_init(void)
{
	qdf_create_work(0, &gp_cds_context->cds_recovery_work,
			cds_trigger_recovery_work, &__cds_recovery_caller);
	gp_cds_context->cds_recovery_wq =
		qdf_create_workqueue("cds_recovery_workqueue");
	if (!gp_cds_context->cds_recovery_wq) {
		cds_err("Failed to create cds_recovery_workqueue");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * cds_recovery_work_deinit() - Initialize recovery work queue
 *
 * Return: none
 */
static void cds_recovery_work_deinit(void)
{
	if (gp_cds_context->cds_recovery_wq) {
		qdf_flush_workqueue(0, gp_cds_context->cds_recovery_wq);
		qdf_destroy_workqueue(0, gp_cds_context->cds_recovery_wq);
	}
}

static bool cds_is_drv_connected(void)
{
	int ret;
	qdf_device_t qdf_ctx;

	qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);
	if (!qdf_ctx) {
		cds_err("cds context is invalid");
		return false;
	}

	ret = pld_is_drv_connected(qdf_ctx->dev);

	return ((ret > 0) ? true : false);
}

static bool cds_is_drv_supported(void)
{
	qdf_device_t qdf_ctx;
	struct pld_platform_cap cap = {0};

	qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);
	if (!qdf_ctx) {
		cds_err("cds context is invalid");
		return false;
	}

	pld_get_platform_cap(qdf_ctx->dev, &cap);

	return ((cap.cap_flag & PLD_HAS_DRV_SUPPORT) ? true : false);
}

static QDF_STATUS cds_wmi_send_recv_qmi(void *buf, uint32_t len, void * cb_ctx,
					qdf_wmi_recv_qmi_cb wmi_rx_cb)
{
	qdf_device_t qdf_ctx;

	qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);
	if (!qdf_ctx) {
		cds_err("cds context is invalid");
		return QDF_STATUS_E_INVAL;
	}

	if (pld_qmi_send(qdf_ctx->dev, 0, buf, len, cb_ctx, wmi_rx_cb))
		return QDF_STATUS_E_INVAL;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS cds_init(void)
{
	QDF_STATUS status;

	gp_cds_context = &g_cds_context;

	status = cds_recovery_work_init();
	if (QDF_IS_STATUS_ERROR(status)) {
		cds_err("Failed to init recovery work; status:%u", status);
		goto deinit;
	}

	cds_ssr_protect_init();

	gp_cds_context->qdf_ctx = &g_qdf_ctx;

	qdf_register_self_recovery_callback(cds_trigger_recovery_psoc);
	qdf_register_fw_down_callback(cds_is_fw_down);
	qdf_register_is_driver_unloading_callback(cds_is_driver_unloading);
	qdf_register_is_driver_state_module_stop_callback(
					cds_is_driver_state_module_stop);
	qdf_register_recovering_state_query_callback(cds_is_driver_recovering);
	qdf_register_drv_connected_callback(cds_is_drv_connected);
	qdf_register_drv_supported_callback(cds_is_drv_supported);
	qdf_register_wmi_send_recv_qmi_callback(cds_wmi_send_recv_qmi);

	return QDF_STATUS_SUCCESS;

deinit:
	gp_cds_context = NULL;
	qdf_mem_zero(&g_cds_context, sizeof(g_cds_context));

	return status;
}

/**
 * cds_deinit() - Deinitialize CDS
 *
 * This function frees the CDS resources
 */
void cds_deinit(void)
{
	QDF_BUG(gp_cds_context);
	if (!gp_cds_context)
		return;

	qdf_register_recovering_state_query_callback(NULL);
	qdf_register_fw_down_callback(NULL);
	qdf_register_is_driver_unloading_callback(NULL);
	qdf_register_is_driver_state_module_stop_callback(NULL);
	qdf_register_self_recovery_callback(NULL);
	qdf_register_wmi_send_recv_qmi_callback(NULL);

	gp_cds_context->qdf_ctx = NULL;
	qdf_mem_zero(&g_qdf_ctx, sizeof(g_qdf_ctx));

	/* currently, no ssr_protect_deinit */

	cds_recovery_work_deinit();

	gp_cds_context = NULL;
	qdf_mem_zero(&g_cds_context, sizeof(g_cds_context));
}

#ifdef FEATURE_WLAN_DIAG_SUPPORT
/**
 * cds_tdls_tx_rx_mgmt_event()- send tdls mgmt rx tx event
 * @event_id: event id
 * @tx_rx: tx or rx
 * @type: type of frame
 * @action_sub_type: action frame type
 * @peer_mac: peer mac
 *
 * This Function sends tdls mgmt rx tx diag event
 *
 * Return: void.
 */
void cds_tdls_tx_rx_mgmt_event(uint8_t event_id, uint8_t tx_rx,
		uint8_t type, uint8_t action_sub_type, uint8_t *peer_mac)
{
	WLAN_HOST_DIAG_EVENT_DEF(tdls_tx_rx_mgmt,
		struct host_event_tdls_tx_rx_mgmt);

	tdls_tx_rx_mgmt.event_id = event_id;
	tdls_tx_rx_mgmt.tx_rx = tx_rx;
	tdls_tx_rx_mgmt.type = type;
	tdls_tx_rx_mgmt.action_sub_type = action_sub_type;
	qdf_mem_copy(tdls_tx_rx_mgmt.peer_mac,
			peer_mac, CDS_MAC_ADDRESS_LEN);
	WLAN_HOST_DIAG_EVENT_REPORT(&tdls_tx_rx_mgmt,
				EVENT_WLAN_TDLS_TX_RX_MGMT);
}
#endif

/**
 * cds_cfg_update_ac_specs_params() - update ac_specs params
 * @olcfg: cfg handle
 * @mac_params: mac params
 *
 * Return: none
 */
static void
cds_cfg_update_ac_specs_params(struct txrx_pdev_cfg_param_t *olcfg,
		struct cds_config_info *cds_cfg)
{
	int i;

	if (!olcfg)
		return;

	if (!cds_cfg)
		return;

	for (i = 0; i < QCA_WLAN_AC_ALL; i++) {
		olcfg->ac_specs[i].wrr_skip_weight =
			cds_cfg->ac_specs[i].wrr_skip_weight;
		olcfg->ac_specs[i].credit_threshold =
			cds_cfg->ac_specs[i].credit_threshold;
		olcfg->ac_specs[i].send_limit =
			cds_cfg->ac_specs[i].send_limit;
		olcfg->ac_specs[i].credit_reserve =
			cds_cfg->ac_specs[i].credit_reserve;
		olcfg->ac_specs[i].discard_weight =
			cds_cfg->ac_specs[i].discard_weight;
	}
}

#if defined(QCA_LL_TX_FLOW_CONTROL_V2) || defined(QCA_LL_PDEV_TX_FLOW_CONTROL)
static inline void
cds_cdp_set_flow_control_params(struct wlan_objmgr_psoc *psoc,
				struct txrx_pdev_cfg_param_t *cdp_cfg)
{
	cdp_cfg->tx_flow_stop_queue_th =
		cfg_get(psoc, CFG_DP_TX_FLOW_STOP_QUEUE_TH);
	cdp_cfg->tx_flow_start_queue_offset =
		cfg_get(psoc, CFG_DP_TX_FLOW_START_QUEUE_OFFSET);
}
#else
static inline void
cds_cdp_set_flow_control_params(struct wlan_objmgr_psoc *psoc,
				struct txrx_pdev_cfg_param_t *cdp_cfg)
{}
#endif

#ifdef QCA_SUPPORT_TXRX_DRIVER_TCP_DEL_ACK
static inline void
cds_cdp_update_del_ack_params(struct wlan_objmgr_psoc *psoc,
			      struct txrx_pdev_cfg_param_t *cdp_cfg)
{
	cdp_cfg->del_ack_enable =
		cfg_get(psoc, CFG_DP_DRIVER_TCP_DELACK_ENABLE);
	cdp_cfg->del_ack_pkt_count =
		cfg_get(psoc, CFG_DP_DRIVER_TCP_DELACK_PKT_CNT);
	cdp_cfg->del_ack_timer_value =
		cfg_get(psoc, CFG_DP_DRIVER_TCP_DELACK_TIMER_VALUE);
}
#else
static inline void
cds_cdp_update_del_ack_params(struct wlan_objmgr_psoc *psoc,
			      struct txrx_pdev_cfg_param_t *cdp_cfg)
{}
#endif

#ifdef WLAN_SUPPORT_TXRX_HL_BUNDLE
static inline void
cds_cdp_update_bundle_params(struct wlan_objmgr_psoc *psoc,
			     struct txrx_pdev_cfg_param_t *cdp_cfg)
{
	cdp_cfg->bundle_timer_value =
		cfg_get(psoc, CFG_DP_HL_BUNDLE_TIMER_VALUE);
	cdp_cfg->bundle_size =
		cfg_get(psoc, CFG_DP_HL_BUNDLE_SIZE);
}
#else
static inline void
cds_cdp_update_bundle_params(struct wlan_objmgr_psoc *psoc,
			     struct txrx_pdev_cfg_param_t *cdp_cfg)
{
}
#endif

/**
 * cds_cdp_cfg_attach() - attach data path config module
 * @cds_cfg: generic platform level config instance
 *
 * Return: none
 */
static void cds_cdp_cfg_attach(struct wlan_objmgr_psoc *psoc)
{
	struct txrx_pdev_cfg_param_t cdp_cfg = {0};
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct hdd_context *hdd_ctx = gp_cds_context->hdd_context;

	cdp_cfg.is_full_reorder_offload =
		cfg_get(psoc, CFG_DP_REORDER_OFFLOAD_SUPPORT);
	cdp_cfg.is_uc_offload_enabled = ucfg_ipa_uc_is_enabled();
	cdp_cfg.uc_tx_buffer_count = cfg_get(psoc, CFG_DP_IPA_UC_TX_BUF_COUNT);
	cdp_cfg.uc_tx_buffer_size =
			cfg_get(psoc, CFG_DP_IPA_UC_TX_BUF_SIZE);
	cdp_cfg.uc_rx_indication_ring_count =
		cfg_get(psoc, CFG_DP_IPA_UC_RX_IND_RING_COUNT);
	cdp_cfg.uc_tx_partition_base =
		cfg_get(psoc, CFG_DP_IPA_UC_TX_PARTITION_BASE);
	cdp_cfg.enable_rxthread = hdd_ctx->enable_rxthread;
	cdp_cfg.ip_tcp_udp_checksum_offload =
		cfg_get(psoc, CFG_DP_TCP_UDP_CKSUM_OFFLOAD);
	cdp_cfg.nan_ip_tcp_udp_checksum_offload =
		cfg_get(psoc, CFG_DP_NAN_TCP_UDP_CKSUM_OFFLOAD);
	cdp_cfg.p2p_ip_tcp_udp_checksum_offload =
		cfg_get(psoc, CFG_DP_P2P_TCP_UDP_CKSUM_OFFLOAD);
	cdp_cfg.legacy_mode_csum_disable =
		cfg_get(psoc, CFG_DP_LEGACY_MODE_CSUM_DISABLE);
	cdp_cfg.ce_classify_enabled =
		cfg_get(psoc, CFG_DP_CE_CLASSIFY_ENABLE);
	cdp_cfg.tso_enable = cfg_get(psoc, CFG_DP_TSO);
	cdp_cfg.lro_enable = cfg_get(psoc, CFG_DP_LRO);
	cdp_cfg.enable_data_stall_detection =
		cfg_get(psoc, CFG_DP_ENABLE_DATA_STALL_DETECTION);
	cdp_cfg.gro_enable = cfg_get(psoc, CFG_DP_GRO);
	cdp_cfg.enable_flow_steering =
		cfg_get(psoc, CFG_DP_FLOW_STEERING_ENABLED);
	cdp_cfg.disable_intra_bss_fwd =
		cfg_get(psoc, CFG_DP_AP_STA_SECURITY_SEPERATION);
	cdp_cfg.pktlog_buffer_size =
		cfg_get(psoc, CFG_DP_PKTLOG_BUFFER_SIZE);

	cds_cdp_update_del_ack_params(psoc, &cdp_cfg);

	cds_cdp_update_bundle_params(psoc, &cdp_cfg);

	gp_cds_context->cfg_ctx = cdp_cfg_attach(soc, gp_cds_context->qdf_ctx,
					(void *)(&cdp_cfg));
	if (!gp_cds_context->cfg_ctx) {
		cds_debug("failed to init cfg handle");
		return;
	}

	/* Configure Receive flow steering */
	cdp_cfg_set_flow_steering(soc, gp_cds_context->cfg_ctx,
				  cfg_get(psoc, CFG_DP_FLOW_STEERING_ENABLED));

	cds_cdp_set_flow_control_params(psoc, &cdp_cfg);
	cdp_cfg_set_flow_control_parameters(soc, gp_cds_context->cfg_ctx,
					    (void *)&cdp_cfg);

	/* adjust the cfg_ctx default value based on setting */
	cdp_cfg_set_rx_fwd_disabled(soc, gp_cds_context->cfg_ctx,
				    cfg_get(psoc,
					    CFG_DP_AP_STA_SECURITY_SEPERATION));

	/*
	 * adjust the packet log enable default value
	 * based on CFG INI setting
	 */
	cdp_cfg_set_packet_log_enabled(soc, gp_cds_context->cfg_ctx,
		(uint8_t)cds_is_packet_log_enabled());

	/* adjust the ptp rx option default value based on CFG INI setting */
	cdp_cfg_set_ptp_rx_opt_enabled(soc, gp_cds_context->cfg_ctx,
				       (uint8_t)cds_is_ptp_rx_opt_enabled());
}
static QDF_STATUS cds_register_all_modules(void)
{
	QDF_STATUS status;

	scheduler_register_wma_legacy_handler(&wma_mc_process_handler);
	scheduler_register_sys_legacy_handler(&sys_mc_process_handler);

	/* Register message queues in given order such that queue priority is
	 * intact:
	 * 1) QDF_MODULE_ID_SYS: Timer queue(legacy SYS queue)
	 * 2) QDF_MODULE_ID_TARGET_IF: Target interface queue
	 * 3) QDF_MODULE_ID_PE: Legacy PE message queue
	 * 4) QDF_MODULE_ID_SME: Legacy SME message queue
	 * 5) QDF_MODULE_ID_OS_IF: OS IF message queue for new components
	 */
	status = scheduler_register_module(QDF_MODULE_ID_SYS,
					&scheduler_timer_q_mq_handler);
	status = scheduler_register_module(QDF_MODULE_ID_TARGET_IF,
					&scheduler_target_if_mq_handler);
	status = scheduler_register_module(QDF_MODULE_ID_PE,
					&pe_mc_process_handler);
	status = scheduler_register_module(QDF_MODULE_ID_SME,
					&sme_mc_process_handler);
	status = scheduler_register_module(QDF_MODULE_ID_OS_IF,
					&scheduler_os_if_mq_handler);
	status = scheduler_register_module(QDF_MODULE_ID_SCAN,
					&scheduler_scan_mq_handler);
	return status;
}

static QDF_STATUS cds_deregister_all_modules(void)
{
	QDF_STATUS status;

	scheduler_deregister_wma_legacy_handler();
	scheduler_deregister_sys_legacy_handler();
	status = scheduler_deregister_module(QDF_MODULE_ID_SCAN);
	status = scheduler_deregister_module(QDF_MODULE_ID_SYS);
	status = scheduler_deregister_module(QDF_MODULE_ID_TARGET_IF);
	status = scheduler_deregister_module(QDF_MODULE_ID_PE);
	status = scheduler_deregister_module(QDF_MODULE_ID_SME);
	status = scheduler_deregister_module(QDF_MODULE_ID_OS_IF);

	return status;
}

/**
 * cds_set_ac_specs_params() - set ac_specs params in cds_config_info
 * @cds_cfg: Pointer to cds_config_info
 * @hdd_ctx: Pointer to hdd context
 *
 * Return: none
 */
static void
cds_set_ac_specs_params(struct cds_config_info *cds_cfg)
{
	int i;
	struct cds_context *cds_ctx;

	if (!cds_cfg)
		return;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);

	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return;
	}

	for (i = 0; i < QCA_WLAN_AC_ALL; i++) {
		cds_cfg->ac_specs[i] = cds_ctx->ac_specs[i];
	}
}

static int cds_hang_event_notifier_call(struct notifier_block *block,
					unsigned long state,
					void *data)
{
	struct qdf_notifer_data *cds_hang_data = data;
	uint32_t total_len;
	struct cds_hang_event_fixed_param *cmd;
	uint8_t *cds_hang_evt_buff;

	if (!cds_hang_data)
		return NOTIFY_STOP_MASK;

	cds_hang_evt_buff = cds_hang_data->hang_data;

	if (!cds_hang_evt_buff)
		return NOTIFY_STOP_MASK;

	total_len = sizeof(*cmd);
	if (cds_hang_data->offset + total_len > QDF_WLAN_HANG_FW_OFFSET)
		return NOTIFY_STOP_MASK;

	cds_hang_evt_buff = cds_hang_data->hang_data + cds_hang_data->offset;
	cmd = (struct cds_hang_event_fixed_param *)cds_hang_evt_buff;
	QDF_HANG_EVT_SET_HDR(&cmd->tlv_header, HANG_EVT_TAG_CDS,
			     QDF_HANG_GET_STRUCT_TLVLEN(*cmd));

	cmd->recovery_reason = gp_cds_context->recovery_reason;

	/* userspace expects a fixed format */
	qdf_mem_set(&cmd->driver_version, DRIVER_VER_LEN, ' ');
	qdf_mem_copy(&cmd->driver_version, QWLAN_VERSIONSTR,
		     qdf_min(sizeof(QWLAN_VERSIONSTR) - 1,
			     (size_t)DRIVER_VER_LEN));

	/* userspace expects a fixed format */
	qdf_mem_set(&cmd->hang_event_version, HANG_EVENT_VER_LEN, ' ');
	qdf_mem_copy(&cmd->hang_event_version, QDF_HANG_EVENT_VERSION,
		     qdf_min(sizeof(QDF_HANG_EVENT_VERSION) - 1,
			     (size_t)HANG_EVENT_VER_LEN));

	cds_hang_data->offset += total_len;
	return NOTIFY_OK;
}

static qdf_notif_block cds_hang_event_notifier = {
	.notif_block.notifier_call = cds_hang_event_notifier_call,
};

/**
 * cds_open() - open the CDS Module
 *
 * cds_open() function opens the CDS Scheduler
 * Upon successful initialization:
 * - All CDS submodules should have been initialized
 *
 * - The CDS scheduler should have opened
 *
 * - All the WLAN SW components should have been opened. This includes
 * SYS, MAC, SME, WMA and TL.
 *
 * Return: QDF status
 */
QDF_STATUS cds_open(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;
	struct cds_config_info *cds_cfg;
	qdf_device_t qdf_ctx;
	struct htc_init_info htcInfo;
	struct ol_context *ol_ctx;
	struct hif_opaque_softc *scn;
	void *HTCHandle;
	struct hdd_context *hdd_ctx;
	struct cds_context *cds_ctx;
	mac_handle_t mac_handle;

	cds_debug("Opening CDS");

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_alert("Trying to open CDS without a PreOpen");
		return QDF_STATUS_E_FAILURE;
	}

	/* Initialize the timer module */
	qdf_timer_module_init();

	/* Initialize bug reporting structure */
	cds_init_log_completion();

	hdd_ctx = gp_cds_context->hdd_context;
	if (!hdd_ctx || !hdd_ctx->config) {
		cds_err("Hdd Context is Null");

		status = QDF_STATUS_E_FAILURE;
		return status;
	}

	status = dispatcher_enable();
	if (QDF_IS_STATUS_ERROR(status)) {
		cds_err("Failed to enable dispatcher; status:%d", status);
		return status;
	}

	/* Now Open the CDS Scheduler */
	status = cds_sched_open(gp_cds_context,
				&gp_cds_context->qdf_sched,
				sizeof(cds_sched_context));
	if (QDF_IS_STATUS_ERROR(status)) {
		cds_alert("Failed to open CDS Scheduler");
		goto err_dispatcher_disable;
	}

	scn = cds_get_context(QDF_MODULE_ID_HIF);
	if (!scn) {
		cds_alert("scn is null!");

		status = QDF_STATUS_E_FAILURE;
		goto err_sched_close;
	}

	cds_cfg = cds_get_ini_config();
	if (!cds_cfg) {
		cds_err("Cds config is NULL");

		status = QDF_STATUS_E_FAILURE;
		goto err_sched_close;
	}

	hdd_enable_fastpath(hdd_ctx, scn);

	/* Initialize BMI and Download firmware */
	ol_ctx = cds_get_context(QDF_MODULE_ID_BMI);
	status = bmi_download_firmware(ol_ctx);
	if (QDF_IS_STATUS_ERROR(status)) {
		cds_alert("BMI FIALED status:%d", status);
		goto err_bmi_close;
	}

	hdd_wlan_update_target_info(hdd_ctx, scn);

	htcInfo.pContext = ol_ctx;
	htcInfo.TargetFailure = ol_target_failure;
	htcInfo.TargetSendSuspendComplete =
		ucfg_pmo_psoc_target_suspend_acknowledge;
	htcInfo.target_initial_wakeup_cb = ucfg_pmo_psoc_handle_initial_wake_up;
	htcInfo.target_psoc = (void *)psoc;
	htcInfo.cfg_wmi_credit_cnt = hdd_ctx->config->cfg_wmi_credit_cnt;
	qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);

	/* Create HTC */
	gp_cds_context->htc_ctx =
		htc_create(scn, &htcInfo, qdf_ctx, cds_get_conparam());
	if (!gp_cds_context->htc_ctx) {
		cds_alert("Failed to Create HTC");

		status = QDF_STATUS_E_FAILURE;
		goto err_bmi_close;
	}
	ucfg_pmo_psoc_update_htc_handle(psoc, (void *)gp_cds_context->htc_ctx);

	status = bmi_done(ol_ctx);
	if (QDF_IS_STATUS_ERROR(status)) {
		cds_alert("Failed to complete BMI phase");
		goto err_htc_close;
	}

	/*Open the WMA module */
	status = wma_open(psoc, hdd_update_tgt_cfg, cds_cfg,
			  hdd_ctx->target_type);
	if (QDF_IS_STATUS_ERROR(status)) {
		cds_alert("Failed to open WMA module");
		goto err_htc_close;
	}

	/* Number of peers limit differs in each chip version. If peer max
	 * limit configured in ini exceeds more than supported, WMA adjusts
	 * and keeps correct limit in cds_cfg.max_station. So, make sure
	 * config entry hdd_ctx->config->maxNumberOfPeers has adjusted value
	 */
	/* In FTM mode cds_cfg->max_stations will be zero. On updating same
	 * into hdd context config entry, leads to pe_open() to fail, if
	 * con_mode change happens from FTM mode to any other mode.
	 */
	if (QDF_DRIVER_TYPE_PRODUCTION == cds_cfg->driver_type)
		ucfg_mlme_set_sap_max_peers(psoc, cds_cfg->max_station);

	HTCHandle = cds_get_context(QDF_MODULE_ID_HTC);
	gp_cds_context->cfg_ctx = NULL;
	if (!HTCHandle) {
		cds_alert("HTCHandle is null!");

		status = QDF_STATUS_E_FAILURE;
		goto err_wma_close;
	}

	status = htc_wait_target(HTCHandle);
	if (QDF_IS_STATUS_ERROR(status)) {
		cds_alert("Failed to complete BMI phase. status: %d", status);
		QDF_BUG(status == QDF_STATUS_E_NOMEM || cds_is_fw_down());

		goto err_wma_close;
	}

	cds_debug("target_type %d 8074:%d 6290:%d 6390: %d 6490: %d 6750: %d",
		  hdd_ctx->target_type,
		  TARGET_TYPE_QCA8074,
		  TARGET_TYPE_QCA6290,
		  TARGET_TYPE_QCA6390,
		  TARGET_TYPE_QCA6490,
		  TARGET_TYPE_QCA6750);

	if (TARGET_TYPE_QCA6290 == hdd_ctx->target_type ||
	    TARGET_TYPE_QCA6390 == hdd_ctx->target_type ||
	    TARGET_TYPE_QCA6490 == hdd_ctx->target_type ||
	    TARGET_TYPE_QCA6750 == hdd_ctx->target_type) {
		gp_cds_context->dp_soc = cdp_soc_attach(LITHIUM_DP,
			gp_cds_context->hif_context, htcInfo.target_psoc,
			gp_cds_context->htc_ctx, gp_cds_context->qdf_ctx,
			&dp_ol_if_ops);

		if (gp_cds_context->dp_soc)
			if (!cdp_soc_init(gp_cds_context->dp_soc, LITHIUM_DP,
					  gp_cds_context->hif_context,
					  htcInfo.target_psoc,
					  gp_cds_context->htc_ctx,
					  gp_cds_context->qdf_ctx,
					  &dp_ol_if_ops)) {
				status = QDF_STATUS_E_FAILURE;
				goto err_soc_detach;
			}
	}
	else
		gp_cds_context->dp_soc = cdp_soc_attach(MOB_DRV_LEGACY_DP,
			gp_cds_context->hif_context, htcInfo.target_psoc,
			gp_cds_context->htc_ctx, gp_cds_context->qdf_ctx,
			&dp_ol_if_ops);

	if (!gp_cds_context->dp_soc) {
		status = QDF_STATUS_E_FAILURE;
		goto err_wma_close;
	}

	wlan_psoc_set_dp_handle(psoc, gp_cds_context->dp_soc);
	ucfg_pmo_psoc_update_dp_handle(psoc, gp_cds_context->dp_soc);
	ucfg_ocb_update_dp_handle(psoc, gp_cds_context->dp_soc);

	cds_set_ac_specs_params(cds_cfg);
	cds_cfg_update_ac_specs_params((struct txrx_pdev_cfg_param_t *)
				       gp_cds_context->cfg_ctx, cds_cfg);
	cds_cdp_cfg_attach(psoc);

	bmi_target_ready(scn, gp_cds_context->cfg_ctx);

	/* Now proceed to open the MAC */
	status = mac_open(psoc, &mac_handle,
			  gp_cds_context->hdd_context, cds_cfg);

	if (QDF_STATUS_SUCCESS != status) {
		cds_alert("Failed to open MAC");
		goto err_soc_deinit;
	}
	gp_cds_context->mac_context = mac_handle;

	/* Now proceed to open the SME */
	status = sme_open(mac_handle);
	if (QDF_IS_STATUS_ERROR(status)) {
		cds_alert("Failed to open SME");
		goto err_mac_close;
	}

	cds_register_all_modules();

	status = dispatcher_psoc_open(psoc);
	if (QDF_IS_STATUS_ERROR(status)) {
		cds_alert("Failed to open PSOC Components");
		goto deregister_modules;
	}

	ucfg_mc_cp_stats_register_pmo_handler();
	qdf_hang_event_register_notifier(&cds_hang_event_notifier);

	return QDF_STATUS_SUCCESS;

deregister_modules:
	cds_deregister_all_modules();
	sme_close(mac_handle);

err_mac_close:
	mac_close(mac_handle);
	gp_cds_context->mac_context = NULL;

err_soc_deinit:
	cdp_soc_deinit(gp_cds_context->dp_soc);

err_soc_detach:
	cdp_soc_detach(gp_cds_context->dp_soc);
	gp_cds_context->dp_soc = NULL;

	ucfg_ocb_update_dp_handle(psoc, NULL);
	ucfg_pmo_psoc_update_dp_handle(psoc, NULL);
	wlan_psoc_set_dp_handle(psoc, NULL);

err_wma_close:
	cds_shutdown_notifier_purge();
	wma_close();
	wma_wmi_service_close();

err_htc_close:
	if (gp_cds_context->htc_ctx) {
		htc_destroy(gp_cds_context->htc_ctx);
		gp_cds_context->htc_ctx = NULL;
		ucfg_pmo_psoc_update_htc_handle(psoc, NULL);
	}

err_bmi_close:
	bmi_cleanup(ol_ctx);

err_sched_close:
	if (QDF_IS_STATUS_ERROR(cds_sched_close()))
		QDF_DEBUG_PANIC("Failed to close CDS Scheduler");

err_dispatcher_disable:
	if (QDF_IS_STATUS_ERROR(dispatcher_disable()))
		QDF_DEBUG_PANIC("Failed to disable dispatcher");

	return status;
} /* cds_open() */

QDF_STATUS cds_dp_open(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS qdf_status;
	struct dp_txrx_config dp_config;
	struct hdd_context *hdd_ctx;


	hdd_ctx = gp_cds_context->hdd_context;
	if (!hdd_ctx) {
		cds_err("HDD context is null");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_status = cdp_pdev_attach(cds_get_context(QDF_MODULE_ID_SOC),
				     gp_cds_context->htc_ctx,
				     gp_cds_context->qdf_ctx, 0);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		/* Critical Error ...  Cannot proceed further */
		cds_alert("Failed to open TXRX");
		QDF_ASSERT(0);
		goto close;
	}

	if (hdd_ctx->target_type == TARGET_TYPE_QCA6290 ||
	    hdd_ctx->target_type == TARGET_TYPE_QCA6390 ||
	    hdd_ctx->target_type == TARGET_TYPE_QCA6490 ||
	    hdd_ctx->target_type == TARGET_TYPE_QCA6750) {
		qdf_status = cdp_pdev_init(cds_get_context(QDF_MODULE_ID_SOC),
					   gp_cds_context->htc_ctx,
					   gp_cds_context->qdf_ctx, 0);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			/* Critical Error ...  Cannot proceed further */
			cds_alert("Failed to init TXRX");
			QDF_ASSERT(0);
			goto pdev_detach;
		}
	}

	if (cdp_txrx_intr_attach(gp_cds_context->dp_soc)
				!= QDF_STATUS_SUCCESS) {
		cds_alert("Failed to attach interrupts");
		goto pdev_deinit;
	}

	dp_config.enable_rx_threads =
		(cds_get_conparam() == QDF_GLOBAL_MONITOR_MODE) ?
		false : gp_cds_context->cds_cfg->enable_dp_rx_threads;

	qdf_status = dp_txrx_init(cds_get_context(QDF_MODULE_ID_SOC),
				  OL_TXRX_PDEV_ID,
				  &dp_config);

	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		goto intr_close;

	ucfg_pmo_psoc_set_txrx_pdev_id(psoc, OL_TXRX_PDEV_ID);
	ucfg_ocb_set_txrx_pdev_id(psoc, OL_TXRX_PDEV_ID);

	cds_debug("CDS successfully Opened");

	return 0;

intr_close:
	cdp_txrx_intr_detach(gp_cds_context->dp_soc);

pdev_deinit:
	cdp_pdev_deinit(gp_cds_context->dp_soc,
			OL_TXRX_PDEV_ID, false);

pdev_detach:
	cdp_pdev_detach(gp_cds_context->dp_soc,
			OL_TXRX_PDEV_ID, false);

close:
	return QDF_STATUS_E_FAILURE;
}

#ifdef HIF_USB
static inline void cds_suspend_target(tp_wma_handle wma_handle)
{
	QDF_STATUS status;
	/* Suspend the target and disable interrupt */
	status = ucfg_pmo_psoc_suspend_target(wma_handle->psoc, 0);
	if (status)
		cds_err("Failed to suspend target, status = %d", status);
}
#else
static inline void cds_suspend_target(tp_wma_handle wma_handle)
{
	QDF_STATUS status;
	/* Suspend the target and disable interrupt */
	status = ucfg_pmo_psoc_suspend_target(wma_handle->psoc, 1);
	if (status)
		cds_err("Failed to suspend target, status = %d", status);
}
#endif /* HIF_USB */

/**
 * cds_pre_enable() - pre enable cds
 *
 * Return: QDF status
 */
QDF_STATUS cds_pre_enable(void)
{
	QDF_STATUS status;
	int errno;
	void *scn;
	void *soc;
	void *hif_ctx;

	cds_enter();

	if (!gp_cds_context) {
		cds_err("cds context is null");
		return QDF_STATUS_E_INVAL;
	}

	if (!gp_cds_context->wma_context) {
		cds_err("wma context is null");
		return QDF_STATUS_E_INVAL;
	}

	scn = cds_get_context(QDF_MODULE_ID_HIF);
	if (!scn) {
		cds_err("hif context is null");
		return QDF_STATUS_E_INVAL;
	}

	soc = cds_get_context(QDF_MODULE_ID_SOC);
	if (!soc) {
		cds_err("soc context is null");
		return QDF_STATUS_E_INVAL;
	}

	/* call Packetlog connect service */
	if (QDF_GLOBAL_FTM_MODE != cds_get_conparam() &&
	    QDF_GLOBAL_EPPING_MODE != cds_get_conparam())
		cdp_pkt_log_con_service(soc, OL_TXRX_PDEV_ID,
					scn);

	/*call WMA pre start */
	status = wma_pre_start();
	if (QDF_IS_STATUS_ERROR(status)) {
		cds_err("Failed to WMA prestart");
		goto exit_pkt_log;
	}

	status = htc_start(gp_cds_context->htc_ctx);
	if (QDF_IS_STATUS_ERROR(status)) {
		cds_err("Failed to Start HTC");
		goto exit_pkt_log;
	}

	status = wma_wait_for_ready_event(gp_cds_context->wma_context);
	if (QDF_IS_STATUS_ERROR(status)) {
		cds_err("Failed to wait for ready event; status: %u", status);
		goto stop_wmi;
	}

	errno = cdp_pdev_post_attach(soc, OL_TXRX_PDEV_ID);
	if (errno) {
		cds_err("Failed to attach pdev");
		status = qdf_status_from_os_return(errno);
		goto stop_wmi;
	}

	return QDF_STATUS_SUCCESS;

stop_wmi:
	/* Send pdev suspend to fw otherwise FW is not aware that
	 * host is freeing resources.
	 */
	if (!(cds_is_driver_recovering() || cds_is_driver_in_bad_state()))
		cds_suspend_target(gp_cds_context->wma_context);

	hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);
	if (!hif_ctx)
		cds_err("Failed to get hif_handle!");

	wma_wmi_stop();

	if (hif_ctx) {
		cds_err("Disable the isr & reset the soc!");
		hif_disable_isr(hif_ctx);
		hif_reset_soc(hif_ctx);
	}
	htc_stop(gp_cds_context->htc_ctx);

	wma_wmi_work_close();

exit_pkt_log:
	if (QDF_GLOBAL_FTM_MODE != cds_get_conparam() &&
	    QDF_GLOBAL_EPPING_MODE != cds_get_conparam())
		cdp_pkt_log_exit(soc, OL_TXRX_PDEV_ID);

	return status;
}

QDF_STATUS cds_enable(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS qdf_status;
	struct mac_start_params mac_params;
	int errno;

	/* We support only one instance for now ... */
	if (!gp_cds_context) {
		cds_err("Invalid CDS context");
		return QDF_STATUS_E_FAILURE;
	}

	if (!gp_cds_context->wma_context) {
		cds_err("WMA NULL context");
		return QDF_STATUS_E_FAILURE;
	}

	if (!gp_cds_context->mac_context) {
		cds_err("MAC NULL context");
		return QDF_STATUS_E_FAILURE;
	}

	/* Start the wma */
	qdf_status = wma_start();
	if (qdf_status != QDF_STATUS_SUCCESS) {
		cds_err("Failed to start wma; status:%d", qdf_status);
		return QDF_STATUS_E_FAILURE;
	}

	/* Start the MAC */
	qdf_mem_zero(&mac_params, sizeof(mac_params));
	mac_params.driver_type = QDF_DRIVER_TYPE_PRODUCTION;
	qdf_status = mac_start(gp_cds_context->mac_context, &mac_params);

	if (QDF_STATUS_SUCCESS != qdf_status) {
		cds_err("Failed to start MAC; status:%d", qdf_status);
		goto err_wma_stop;
	}

	/* START SME */
	qdf_status = sme_start(gp_cds_context->mac_context);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		cds_err("Failed to start SME; status:%d", qdf_status);
		goto err_mac_stop;
	}

	qdf_status = cdp_soc_attach_target(cds_get_context(QDF_MODULE_ID_SOC));
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		cds_err("Failed to attach soc target; status:%d", qdf_status);
		goto err_sme_stop;
	}

	errno = cdp_pdev_attach_target(cds_get_context(QDF_MODULE_ID_SOC),
				       OL_TXRX_PDEV_ID);
	if (errno) {
		cds_err("Failed to attach pdev target; errno:%d", errno);
		goto err_soc_target_detach;
	}

	qdf_status = dispatcher_psoc_enable(psoc);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		cds_err("dispatcher_psoc_enable failed; status:%d", qdf_status);
		goto err_soc_target_detach;
	}

	/* Trigger psoc enable for CLD components */
	hdd_component_psoc_enable(psoc);

	return QDF_STATUS_SUCCESS;

err_soc_target_detach:
	/* NOOP */

err_sme_stop:
	sme_stop(gp_cds_context->mac_context);

err_mac_stop:
	mac_stop(gp_cds_context->mac_context);

err_wma_stop:
	qdf_status = wma_stop();
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		cds_err("Failed to stop wma");
		QDF_ASSERT(QDF_IS_STATUS_SUCCESS(qdf_status));
	}

	return QDF_STATUS_E_FAILURE;
} /* cds_enable() */

/**
 * cds_disable() - stop/disable cds module
 * @psoc: Psoc pointer
 *
 * Return: QDF status
 */
QDF_STATUS cds_disable(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS qdf_status;
	void *handle;

	/* PSOC disable for all new components. It needs to happen before
	 * target is PDEV suspended such that a component can abort all its
	 * ongoing transaction with FW. Always keep it before wma_stop() as
	 * wma_stop() does target PDEV suspend.
	 */

	/* Trigger psoc disable for CLD components */
	if (psoc) {
		hdd_component_psoc_disable(psoc);
		dispatcher_psoc_disable(psoc);
	}

	qdf_status = wma_stop();

	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		cds_err("Failed to stop wma");
		QDF_ASSERT(QDF_IS_STATUS_SUCCESS(qdf_status));
	}

	handle = cds_get_context(QDF_MODULE_ID_PE);
	if (!handle) {
		cds_err("Invalid PE context return!");
		return QDF_STATUS_E_INVAL;
	}

	umac_stop();

	return qdf_status;
}

/**
 * cds_post_disable() - post disable cds module
 *
 * Return: QDF status
 */
QDF_STATUS cds_post_disable(void)
{
	tp_wma_handle wma_handle;
	struct hif_opaque_softc *hif_ctx;
	struct scheduler_ctx *sched_ctx;
	QDF_STATUS qdf_status;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		cds_err("Failed to get wma_handle!");
		return QDF_STATUS_E_INVAL;
	}

	hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);
	if (!hif_ctx) {
		cds_err("Failed to get hif_handle!");
		return QDF_STATUS_E_INVAL;
	}

	/* flush any unprocessed scheduler messages */
	sched_ctx = scheduler_get_context();
	if (sched_ctx)
		scheduler_queues_flush(sched_ctx);

	/*
	 * With new state machine changes cds_close can be invoked without
	 * cds_disable. So, send the following clean up prerequisites to fw,
	 * So Fw and host are in sync for cleanup indication:
	 * - Send PDEV_SUSPEND indication to firmware
	 * - Disable HIF Interrupts.
	 * - Clean up CE tasklets.
	 */

	cds_debug("send deinit sequence to firmware");
	if (!(cds_is_driver_recovering() || cds_is_driver_in_bad_state()))
		cds_suspend_target(wma_handle);
	hif_disable_isr(hif_ctx);
	hif_reset_soc(hif_ctx);

	if (gp_cds_context->htc_ctx) {
		wma_wmi_stop();
		htc_stop(gp_cds_context->htc_ctx);
	}

	qdf_status = cds_close_rx_thread();
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		cds_err("Failed to close RX thread!");
		return QDF_STATUS_E_INVAL;
	}

	cdp_pdev_pre_detach(cds_get_context(QDF_MODULE_ID_SOC),
			    OL_TXRX_PDEV_ID, 1);

	return QDF_STATUS_SUCCESS;
}

/**
 * cds_close() - close cds module
 * @psoc: Psoc pointer
 *
 * This API allows user to close modules registered
 * with connectivity device services.
 *
 * Return: QDF status
 */
QDF_STATUS cds_close(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS qdf_status;

	qdf_hang_event_unregister_notifier(&cds_hang_event_notifier);
	qdf_status = cds_sched_close();
	QDF_ASSERT(QDF_IS_STATUS_SUCCESS(qdf_status));
	if (QDF_IS_STATUS_ERROR(qdf_status))
		cds_err("Failed to close CDS Scheduler");

	qdf_status = dispatcher_disable();
	QDF_ASSERT(QDF_IS_STATUS_SUCCESS(qdf_status));
	if (QDF_IS_STATUS_ERROR(qdf_status))
		cds_err("Failed to disable dispatcher; status:%d", qdf_status);

	dispatcher_psoc_close(psoc);

	qdf_flush_work(&gp_cds_context->cds_recovery_work);

	cds_shutdown_notifier_purge();

	qdf_status = wma_wmi_work_close();
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		cds_err("Failed to close wma_wmi_work");
		QDF_ASSERT(0);
	}

	if (gp_cds_context->htc_ctx) {
		htc_destroy(gp_cds_context->htc_ctx);
		ucfg_pmo_psoc_update_htc_handle(psoc, NULL);
		gp_cds_context->htc_ctx = NULL;
	}

	qdf_status = sme_close(gp_cds_context->mac_context);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		cds_err("Failed to close SME");
		QDF_ASSERT(QDF_IS_STATUS_SUCCESS(qdf_status));
	}

	qdf_status = mac_close(gp_cds_context->mac_context);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		cds_err("Failed to close MAC");
		QDF_ASSERT(QDF_IS_STATUS_SUCCESS(qdf_status));
	}

	gp_cds_context->mac_context = NULL;

	cdp_soc_deinit(gp_cds_context->dp_soc);
	cdp_soc_detach(gp_cds_context->dp_soc);
	gp_cds_context->dp_soc = NULL;

	ucfg_pmo_psoc_update_dp_handle(psoc, NULL);
	wlan_psoc_set_dp_handle(psoc, NULL);


	qdf_status = wma_close();
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		cds_err("Failed to close wma");
		QDF_ASSERT(QDF_IS_STATUS_SUCCESS(qdf_status));
	}

	qdf_status = wma_wmi_service_close();
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		cds_err("Failed to close wma_wmi_service");
		QDF_ASSERT(QDF_IS_STATUS_SUCCESS(qdf_status));
	}

	cds_deinit_ini_config();
	qdf_timer_module_deinit();

	cds_deregister_all_modules();

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS cds_dp_close(struct wlan_objmgr_psoc *psoc)
{
	cdp_txrx_intr_detach(gp_cds_context->dp_soc);

	dp_txrx_deinit(cds_get_context(QDF_MODULE_ID_SOC));

	cdp_pdev_deinit(cds_get_context(QDF_MODULE_ID_SOC), OL_TXRX_PDEV_ID, 1);

	cdp_pdev_detach(cds_get_context(QDF_MODULE_ID_SOC), OL_TXRX_PDEV_ID, 1);

	ucfg_pmo_psoc_set_txrx_pdev_id(psoc, OL_TXRX_INVALID_PDEV_ID);

	return QDF_STATUS_SUCCESS;
}

/**
 * cds_get_context() - get context data area
 *
 * @module_id: ID of the module who's context data is being retrieved.
 *
 * Each module in the system has a context / data area that is allocated
 * and managed by CDS.  This API allows any user to get a pointer to its
 * allocated context data area from the CDS global context.
 *
 * Return: pointer to the context data area of the module ID
 *	   specified, or NULL if the context data is not allocated for
 *	   the module ID specified
 */
void *cds_get_context(QDF_MODULE_ID module_id)
{
	void *context = NULL;

	if (!gp_cds_context) {
		cds_err("cds context pointer is null");
		return NULL;
	}

	switch (module_id) {
	case QDF_MODULE_ID_HDD:
	{
		context = gp_cds_context->hdd_context;
		break;
	}

	case QDF_MODULE_ID_SME:
	case QDF_MODULE_ID_PE:
	{
		/* In all these cases, we just return the MAC Context */
		context = gp_cds_context->mac_context;
		break;
	}

	case QDF_MODULE_ID_WMA:
	{
		/* For wma module */
		context = gp_cds_context->wma_context;
		break;
	}

	case QDF_MODULE_ID_QDF:
	{
		/* For SYS this is CDS itself */
		context = gp_cds_context;
		break;
	}

	case QDF_MODULE_ID_HIF:
	{
		context = gp_cds_context->hif_context;
		break;
	}

	case QDF_MODULE_ID_HTC:
	{
		context = gp_cds_context->htc_ctx;
		break;
	}

	case QDF_MODULE_ID_QDF_DEVICE:
	{
		context = gp_cds_context->qdf_ctx;
		break;
	}

	case QDF_MODULE_ID_BMI:
	{
		context = gp_cds_context->g_ol_context;
		break;
	}

	case QDF_MODULE_ID_CFG:
	{
		context = gp_cds_context->cfg_ctx;
		break;
	}

	case QDF_MODULE_ID_SOC:
	{
		context = gp_cds_context->dp_soc;
		break;
	}

	default:
	{
		cds_err("Module ID %i does not have its context maintained by CDS",
			module_id);
		QDF_ASSERT(0);
		return NULL;
	}
	}

	if (!context)
		cds_err("Module ID %i context is Null", module_id);

	return context;
} /* cds_get_context() */

/**
 * cds_get_global_context() - get CDS global Context
 *
 * This API allows any user to get the CDS Global Context pointer from a
 * module context data area.
 *
 * Return: pointer to the CDS global context, NULL if the function is
 *	   unable to retrieve the CDS context.
 */
void *cds_get_global_context(void)
{
	if (!gp_cds_context) {
		/*
		 * To avoid recursive call, this should not change to
		 * QDF_TRACE().
		 */
		pr_err("%s: global cds context is NULL", __func__);
	}

	return gp_cds_context;
} /* cds_get_global_context() */

/**
 * cds_get_driver_state() - Get current driver state
 *
 * This API returns current driver state stored in global context.
 *
 * Return: Driver state enum
 */
enum cds_driver_state cds_get_driver_state(void)
{
	if (!gp_cds_context) {
		cds_err("global cds context is NULL");

		return CDS_DRIVER_STATE_UNINITIALIZED;
	}

	return gp_cds_context->driver_state;
}

/**
 * cds_set_driver_state() - Set current driver state
 * @state:	Driver state to be set to.
 *
 * This API sets driver state to state. This API only sets the state and doesn't
 * clear states, please make sure to use cds_clear_driver_state to clear any
 * state if required.
 *
 * Return: None
 */
void cds_set_driver_state(enum cds_driver_state state)
{
	if (!gp_cds_context) {
		cds_err("global cds context is NULL: %x", state);

		return;
	}

	gp_cds_context->driver_state |= state;
}

/**
 * cds_clear_driver_state() - Clear current driver state
 * @state:	Driver state to be cleared.
 *
 * This API clears driver state. This API only clears the state, please make
 * sure to use cds_set_driver_state to set any new states.
 *
 * Return: None
 */
void cds_clear_driver_state(enum cds_driver_state state)
{
	if (!gp_cds_context) {
		cds_err("global cds context is NULL: %x", state);

		return;
	}

	gp_cds_context->driver_state &= ~state;
}

/**
 * cds_alloc_context() - allocate a context within the CDS global Context
 * @module_id: module ID who's context area is being allocated.
 * @module_context: pointer to location where the pointer to the
 *	allocated context is returned. Note this output pointer
 *	is valid only if the API returns QDF_STATUS_SUCCESS
 * @param size: size of the context area to be allocated.
 *
 * This API allows any user to allocate a user context area within the
 * CDS Global Context.
 *
 * Return: QDF status
 */
QDF_STATUS cds_alloc_context(QDF_MODULE_ID module_id,
			     void **module_context, uint32_t size)
{
	void **cds_mod_context = NULL;

	if (!gp_cds_context) {
		cds_err("cds context is null");
		return QDF_STATUS_E_FAILURE;
	}

	if (!module_context) {
		cds_err("null param passed");
		return QDF_STATUS_E_FAILURE;
	}

	switch (module_id) {
	case QDF_MODULE_ID_WMA:
		cds_mod_context = &gp_cds_context->wma_context;
		break;

	case QDF_MODULE_ID_HIF:
		cds_mod_context = &gp_cds_context->hif_context;
		break;

	case QDF_MODULE_ID_BMI:
		cds_mod_context = &gp_cds_context->g_ol_context;
		break;

	default:
		cds_err("Module ID %i does not have its context allocated by CDS",
			module_id);
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	if (*cds_mod_context) {
		/* Context has already been allocated!
		 * Prevent double allocation
		 */
		cds_err("Module ID %i context has already been allocated",
			module_id);
		return QDF_STATUS_E_EXISTS;
	}

	/* Dynamically allocate the context for module */

	*module_context = qdf_mem_malloc(size);

	if (!*module_context) {
		cds_err("Failed to allocate Context for module ID %i",
			module_id);
		QDF_ASSERT(0);
		return QDF_STATUS_E_NOMEM;
	}

	*cds_mod_context = *module_context;

	return QDF_STATUS_SUCCESS;
} /* cds_alloc_context() */

/**
 * cds_set_context() - API to set context in global CDS Context
 * @module_id: Module ID
 * @context: Pointer to the Module Context
 *
 * API to set a MODULE Context in global CDS Context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cds_set_context(QDF_MODULE_ID module_id, void *context)
{
	struct cds_context *p_cds_context = cds_get_global_context();

	if (!p_cds_context) {
		cds_err("cds context is Invalid");
		return QDF_STATUS_NOT_INITIALIZED;
	}

	switch (module_id) {
	case QDF_MODULE_ID_HDD:
		p_cds_context->hdd_context = context;
		break;
	case QDF_MODULE_ID_HIF:
		p_cds_context->hif_context = context;
		break;
	default:
		cds_err("Module ID %i does not have its context managed by CDS",
			module_id);
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * cds_free_context() - free an allocated context within the
 *			CDS global Context
 * @module_id: module ID who's context area is being free
 * @module_context: pointer to module context area to be free'd.
 *
 *  This API allows a user to free the user context area within the
 *  CDS Global Context.
 *
 * Return: QDF status
 */
QDF_STATUS cds_free_context(QDF_MODULE_ID module_id, void *module_context)
{
	void **cds_mod_context = NULL;

	if (!gp_cds_context) {
		cds_err("cds context is null");
		return QDF_STATUS_E_FAILURE;
	}

	if (!module_context) {
		cds_err("Null param");
		return QDF_STATUS_E_FAILURE;
	}

	switch (module_id) {
	case QDF_MODULE_ID_WMA:
		cds_mod_context = &gp_cds_context->wma_context;
		break;

	case QDF_MODULE_ID_HIF:
		cds_mod_context = &gp_cds_context->hif_context;
		break;

	case QDF_MODULE_ID_BMI:
		cds_mod_context = &gp_cds_context->g_ol_context;
		break;

	default:
		cds_err("Module ID %i does not have its context allocated by CDS",
			module_id);
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	if (!*cds_mod_context) {
		/* Context has not been allocated or freed already! */
		cds_err("Module ID %i context has not been allocated or freed already",
			module_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (*cds_mod_context != module_context) {
		cds_err("cds_mod_context != module_context");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mem_free(module_context);

	*cds_mod_context = NULL;

	return QDF_STATUS_SUCCESS;
} /* cds_free_context() */


/**
 * cds_flush_work() - flush pending works
 * @work: pointer to work
 *
 * Return: none
 */
void cds_flush_work(void *work)
{
	cancel_work_sync(work);
}

/**
 * cds_flush_delayed_work() - flush delayed works
 * @dwork: pointer to delayed work
 *
 * Return: none
 */
void cds_flush_delayed_work(void *dwork)
{
	cancel_delayed_work_sync(dwork);
}

#ifndef REMOVE_PKT_LOG
/**
 * cds_is_packet_log_enabled() - check if packet log is enabled
 *
 * Return: true if packet log is enabled else false
 */
bool cds_is_packet_log_enabled(void)
{
	struct hdd_context *hdd_ctx;

	hdd_ctx = gp_cds_context->hdd_context;
	if ((!hdd_ctx) || (!hdd_ctx->config)) {
		cds_alert("Hdd Context is Null");
		return false;
	}
	return hdd_ctx->config->enable_packet_log;
}
#endif

static int cds_force_assert_target_via_pld(qdf_device_t qdf)
{
	int errno;

	errno = pld_force_assert_target(qdf->dev);
	if (errno == -EOPNOTSUPP)
		cds_info("PLD does not support target force assert");
	else if (errno)
		cds_err("Failed PLD target force assert; errno %d", errno);
	else
		cds_info("Target force assert triggered via PLD");

	return errno;
}

static QDF_STATUS cds_force_assert_target_via_wmi(qdf_device_t qdf)
{
	QDF_STATUS status;
	t_wma_handle *wma;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		cds_err("wma is null");
		return QDF_STATUS_E_INVAL;
	}

	status = wma_crash_inject(wma, RECOVERY_SIM_SELF_RECOVERY, 0);
	if (QDF_IS_STATUS_ERROR(status)) {
		cds_err("Failed target force assert; status %d", status);
		return status;
	}

	status = qdf_wait_for_event_completion(&wma->recovery_event,
				       WMA_CRASH_INJECT_TIMEOUT);
	if (QDF_IS_STATUS_ERROR(status)) {
		cds_err("Failed target force assert wait; status %d", status);
		return status;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * cds_force_assert_target() - Send assert command to firmware
 * @qdf: QDF device instance to assert
 *
 * An out-of-band recovery mechanism will cleanup and restart the entire wlan
 * subsystem in the event of a firmware crash. This API injects a firmware
 * crash to start this process when the wlan driver is known to be in a bad
 * state. If a firmware assert inject fails, the wlan driver will schedule
 * the driver recovery anyway, as a best effort attempt to return to a working
 * state.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS cds_force_assert_target(qdf_device_t qdf)
{
	int errno;
	QDF_STATUS status;

	/* first, try target assert inject via pld */
	errno = cds_force_assert_target_via_pld(qdf);
	if (!errno)
		return QDF_STATUS_SUCCESS;
	if (errno != -EOPNOTSUPP)
		return QDF_STATUS_E_FAILURE;

	/* pld assert is not supported, try target assert inject via wmi */
	status = cds_force_assert_target_via_wmi(qdf);
	if (QDF_IS_STATUS_SUCCESS(status))
		return QDF_STATUS_SUCCESS;

	/* wmi assert failed, start recovery without the firmware assert */
	cds_err("Scheduling recovery work without firmware assert");
	pld_schedule_recovery_work(qdf->dev, PLD_REASON_DEFAULT);

	return status;
}

/**
 * cds_trigger_recovery_handler() - handle a self recovery request
 * @func: the name of the function that called cds_trigger_recovery
 * @line: the line number of the call site which called cds_trigger_recovery
 *
 * Return: none
 */
static void cds_trigger_recovery_handler(const char *func, const uint32_t line)
{
	QDF_STATUS status;
	qdf_runtime_lock_t rtl;
	qdf_device_t qdf;
	bool ssr_ini_enabled = cds_is_self_recovery_enabled();

	/* NOTE! This code path is delicate! Think very carefully before
	 * modifying the content or order of the following. Please review any
	 * potential changes with someone closely familiar with this feature.
	 */

	if (cds_is_driver_recovering()) {
		cds_info("WLAN recovery already in progress");
		return;
	}

	if (cds_is_driver_in_bad_state()) {
		cds_info("WLAN has already failed recovery");
		return;
	}

	if (cds_is_fw_down()) {
		cds_info("Firmware has already initiated recovery");
		return;
	}

	qdf = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);
	if (!qdf) {
		cds_err("Qdf context is null");
		return;
	}

	/*
	 * if *wlan* recovery is disabled, crash here for debugging  for
	 * snoc/IPCI targets.
	 */
	if ((qdf->bus_type == QDF_BUS_TYPE_SNOC ||
	     qdf->bus_type == QDF_BUS_TYPE_IPCI) && !ssr_ini_enabled) {
		QDF_DEBUG_PANIC("WLAN recovery is not enabled (via %s:%d)",
				func, line);
		return;
	}

	if (cds_is_driver_unloading()) {
		QDF_DEBUG_PANIC("WLAN is unloading recovery not expected(via %s:%d)",
				func, line);
		return;
	}

	status = qdf_runtime_lock_init(&rtl);
	if (QDF_IS_STATUS_ERROR(status)) {
		cds_err("qdf_runtime_lock_init failed, status: %d", status);
		return;
	}

	status = qdf_runtime_pm_prevent_suspend(&rtl);
	if (QDF_IS_STATUS_ERROR(status)) {
		cds_err("Failed to acquire runtime pm lock");
		goto deinit_rtl;
	}

	cds_set_recovery_in_progress(true);
	cds_set_assert_target_in_progress(true);
	if (pld_force_collect_target_dump(qdf->dev))
		cds_force_assert_target(qdf);
	cds_set_assert_target_in_progress(false);

	/*
	 * if *wlan* recovery is disabled, once all the required registers are
	 * read via the platform driver check and crash the system.
	 */
	if (qdf->bus_type == QDF_BUS_TYPE_PCI && !ssr_ini_enabled)
		QDF_DEBUG_PANIC("WLAN recovery is not enabled (via %s:%d)",
				func, line);

	status = qdf_runtime_pm_allow_suspend(&rtl);
	if (QDF_IS_STATUS_ERROR(status))
		cds_err("Failed to release runtime pm lock");

deinit_rtl:
	qdf_runtime_lock_deinit(&rtl);
}

static void cds_trigger_recovery_work(void *context)
{
	struct cds_recovery_call_info *call_info = context;

	cds_trigger_recovery_handler(call_info->func, call_info->line);
}

void __cds_trigger_recovery(enum qdf_hang_reason reason, const char *func,
			    const uint32_t line)
{
	if (!gp_cds_context) {
		cds_err("gp_cds_context is null");
		return;
	}

	gp_cds_context->recovery_reason = reason;

	__cds_recovery_caller.func = func;
	__cds_recovery_caller.line = line;
	qdf_queue_work(0, gp_cds_context->cds_recovery_wq,
		       &gp_cds_context->cds_recovery_work);
}

void cds_trigger_recovery_psoc(void *psoc, enum qdf_hang_reason reason,
			       const char *func, const uint32_t line)
{
	__cds_trigger_recovery(reason, func, line);
}


/**
 * cds_get_recovery_reason() - get self recovery reason
 * @reason: recovery reason
 *
 * Return: None
 */
void cds_get_recovery_reason(enum qdf_hang_reason *reason)
{
	if (!gp_cds_context) {
		cds_err("gp_cds_context is null");
		return;
	}

	*reason = gp_cds_context->recovery_reason;
}

/**
 * cds_reset_recovery_reason() - reset the reason to unspecified
 *
 * Return: None
 */
void cds_reset_recovery_reason(void)
{
	if (!gp_cds_context) {
		cds_err("gp_cds_context is null");
		return;
	}

	gp_cds_context->recovery_reason = QDF_REASON_UNSPECIFIED;
}

/**
 * cds_set_wakelock_logging() - Logging of wakelock enabled/disabled
 * @value: Boolean value
 *
 * This function is used to set the flag which will indicate whether
 * logging of wakelock is enabled or not
 *
 * Return: None
 */
void cds_set_wakelock_logging(bool value)
{
	struct cds_context *p_cds_context;

	p_cds_context = cds_get_global_context();
	if (!p_cds_context) {
		cds_err("cds context is Invald");
		return;
	}
	p_cds_context->is_wakelock_log_enabled = value;
}

/**
 * cds_is_wakelock_enabled() - Check if logging of wakelock is enabled/disabled
 * @value: Boolean value
 *
 * This function is used to check whether logging of wakelock is enabled or not
 *
 * Return: true if logging of wakelock is enabled
 */
bool cds_is_wakelock_enabled(void)
{
	struct cds_context *p_cds_context;

	p_cds_context = cds_get_global_context();
	if (!p_cds_context) {
		cds_err("cds context is Invald");
		return false;
	}
	return p_cds_context->is_wakelock_log_enabled;
}

/**
 * cds_set_ring_log_level() - Sets the log level of a particular ring
 * @ring_id: ring_id
 * @log_levelvalue: Log level specificed
 *
 * This function converts HLOS values to driver log levels and sets the log
 * level of a particular ring accordingly.
 *
 * Return: None
 */
void cds_set_ring_log_level(uint32_t ring_id, uint32_t log_level)
{
	struct cds_context *p_cds_context;
	uint32_t log_val;

	p_cds_context = cds_get_global_context();
	if (!p_cds_context) {
		cds_err("cds context is Invald");
		return;
	}

	switch (log_level) {
	case LOG_LEVEL_NO_COLLECTION:
		log_val = WLAN_LOG_LEVEL_OFF;
		break;
	case LOG_LEVEL_NORMAL_COLLECT:
		log_val = WLAN_LOG_LEVEL_NORMAL;
		break;
	case LOG_LEVEL_ISSUE_REPRO:
		log_val = WLAN_LOG_LEVEL_REPRO;
		break;
	case LOG_LEVEL_ACTIVE:
	default:
		log_val = WLAN_LOG_LEVEL_ACTIVE;
		break;
	}

	if (ring_id == RING_ID_WAKELOCK) {
		p_cds_context->wakelock_log_level = log_val;
		return;
	} else if (ring_id == RING_ID_CONNECTIVITY) {
		p_cds_context->connectivity_log_level = log_val;
		return;
	} else if (ring_id == RING_ID_PER_PACKET_STATS) {
		p_cds_context->packet_stats_log_level = log_val;
		return;
	} else if (ring_id == RING_ID_DRIVER_DEBUG) {
		p_cds_context->driver_debug_log_level = log_val;
		return;
	} else if (ring_id == RING_ID_FIRMWARE_DEBUG) {
		p_cds_context->fw_debug_log_level = log_val;
		return;
	}
}

/**
 * cds_get_ring_log_level() - Get the a ring id's log level
 * @ring_id: Ring id
 *
 * Fetch and return the log level corresponding to a ring id
 *
 * Return: Log level corresponding to the ring ID
 */
enum wifi_driver_log_level cds_get_ring_log_level(uint32_t ring_id)
{
	struct cds_context *p_cds_context;

	p_cds_context = cds_get_global_context();
	if (!p_cds_context) {
		cds_err("cds context is Invald");
		return WLAN_LOG_LEVEL_OFF;
	}

	if (ring_id == RING_ID_WAKELOCK)
		return p_cds_context->wakelock_log_level;
	else if (ring_id == RING_ID_CONNECTIVITY)
		return p_cds_context->connectivity_log_level;
	else if (ring_id == RING_ID_PER_PACKET_STATS)
		return p_cds_context->packet_stats_log_level;
	else if (ring_id == RING_ID_DRIVER_DEBUG)
		return p_cds_context->driver_debug_log_level;
	else if (ring_id == RING_ID_FIRMWARE_DEBUG)
		return p_cds_context->fw_debug_log_level;

	return WLAN_LOG_LEVEL_OFF;
}

/**
 * cds_set_multicast_logging() - Set mutlicast logging value
 * @value: Value of multicast logging
 *
 * Set the multicast logging value which will indicate
 * whether to multicast host and fw messages even
 * without any registration by userspace entity
 *
 * Return: None
 */
void cds_set_multicast_logging(uint8_t value)
{
	cds_multicast_logging = value;
}

/**
 * cds_is_multicast_logging() - Get multicast logging value
 *
 * Get the multicast logging value which will indicate
 * whether to multicast host and fw messages even
 * without any registration by userspace entity
 *
 * Return: 0 - Multicast logging disabled, 1 - Multicast logging enabled
 */
uint8_t cds_is_multicast_logging(void)
{
	return cds_multicast_logging;
}

/*
 * cds_init_log_completion() - Initialize log param structure
 *
 * This function is used to initialize the logging related
 * parameters
 *
 * Return: None
 */
void cds_init_log_completion(void)
{
	struct cds_context *p_cds_context;

	p_cds_context = cds_get_global_context();
	if (!p_cds_context) {
		cds_err("cds context is Invalid");
		return;
	}

	p_cds_context->log_complete.is_fatal = WLAN_LOG_TYPE_NON_FATAL;
	p_cds_context->log_complete.indicator = WLAN_LOG_INDICATOR_UNUSED;
	p_cds_context->log_complete.reason_code = WLAN_LOG_REASON_CODE_UNUSED;
	p_cds_context->log_complete.is_report_in_progress = false;
}

/**
 * cds_set_log_completion() - Store the logging params
 * @is_fatal: Indicates if the event triggering bug report is fatal or not
 * @indicator: Source which trigerred the bug report
 * @reason_code: Reason for triggering bug report
 * @recovery_needed: If recovery is needed after bug report
 *
 * This function is used to set the logging parameters based on the
 * caller
 *
 * Return: 0 if setting of params is successful
 */
QDF_STATUS cds_set_log_completion(uint32_t is_fatal,
		uint32_t indicator,
		uint32_t reason_code,
		bool recovery_needed)
{
	struct cds_context *p_cds_context;

	p_cds_context = cds_get_global_context();
	if (!p_cds_context) {
		cds_err("cds context is Invalid");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_spinlock_acquire(&p_cds_context->bug_report_lock);
	p_cds_context->log_complete.is_fatal = is_fatal;
	p_cds_context->log_complete.indicator = indicator;
	p_cds_context->log_complete.reason_code = reason_code;
	p_cds_context->log_complete.recovery_needed = recovery_needed;
	p_cds_context->log_complete.is_report_in_progress = true;
	qdf_spinlock_release(&p_cds_context->bug_report_lock);
	cds_debug("is_fatal %d indicator %d reason_code %d recovery needed %d",
		  is_fatal, indicator, reason_code, recovery_needed);

	return QDF_STATUS_SUCCESS;
}

/**
 * cds_get_and_reset_log_completion() - Get and reset logging related params
 * @is_fatal: Indicates if the event triggering bug report is fatal or not
 * @indicator: Source which trigerred the bug report
 * @reason_code: Reason for triggering bug report
 * @recovery_needed: If recovery is needed after bug report
 *
 * This function is used to get the logging related parameters
 *
 * Return: None
 */
void cds_get_and_reset_log_completion(uint32_t *is_fatal,
		uint32_t *indicator,
		uint32_t *reason_code,
		bool *recovery_needed)
{
	struct cds_context *p_cds_context;

	p_cds_context = cds_get_global_context();
	if (!p_cds_context) {
		cds_err("cds context is Invalid");
		return;
	}

	qdf_spinlock_acquire(&p_cds_context->bug_report_lock);
	*is_fatal =  p_cds_context->log_complete.is_fatal;
	*indicator = p_cds_context->log_complete.indicator;
	*reason_code = p_cds_context->log_complete.reason_code;
	*recovery_needed = p_cds_context->log_complete.recovery_needed;

	/* reset */
	p_cds_context->log_complete.indicator = WLAN_LOG_INDICATOR_UNUSED;
	p_cds_context->log_complete.is_fatal = WLAN_LOG_TYPE_NON_FATAL;
	p_cds_context->log_complete.is_report_in_progress = false;
	p_cds_context->log_complete.reason_code = WLAN_LOG_REASON_CODE_UNUSED;
	p_cds_context->log_complete.recovery_needed = false;
	qdf_spinlock_release(&p_cds_context->bug_report_lock);
}

/**
 * cds_is_log_report_in_progress() - Check if bug reporting is in progress
 *
 * This function is used to check if the bug reporting is already in progress
 *
 * Return: true if the bug reporting is in progress
 */
bool cds_is_log_report_in_progress(void)
{
	struct cds_context *p_cds_context;

	p_cds_context = cds_get_global_context();
	if (!p_cds_context) {
		cds_err("cds context is Invalid");
		return true;
	}
	return p_cds_context->log_complete.is_report_in_progress;
}

/**
 * cds_is_fatal_event_enabled() - Return if fatal event is enabled
 *
 * Return true if fatal event is enabled.
 */
bool cds_is_fatal_event_enabled(void)
{
	struct cds_context *p_cds_context;

	p_cds_context = cds_get_global_context();
	if (!p_cds_context) {
		cds_err("cds context is Invalid");
		return false;
	}


	return p_cds_context->enable_fatal_event;
}

#ifdef WLAN_FEATURE_TSF_PLUS
bool cds_is_ptp_rx_opt_enabled(void)
{
	struct hdd_context *hdd_ctx;
	struct cds_context *p_cds_context;

	p_cds_context = cds_get_global_context();
	if (!p_cds_context) {
		cds_err("cds context is Invalid");
		return false;
	}

	hdd_ctx = (struct hdd_context *)(p_cds_context->hdd_context);
	if ((!hdd_ctx) || (!hdd_ctx->config)) {
		cds_err("Hdd Context is Null");
		return false;
	}

	return hdd_tsf_is_rx_set(hdd_ctx);
}

bool cds_is_ptp_tx_opt_enabled(void)
{
	struct hdd_context *hdd_ctx;
	struct cds_context *p_cds_context;

	p_cds_context = cds_get_global_context();
	if (!p_cds_context) {
		cds_err("cds context is Invalid");
		return false;
	}

	hdd_ctx = (struct hdd_context *)(p_cds_context->hdd_context);
	if ((!hdd_ctx) || (!hdd_ctx->config)) {
		cds_err("Hdd Context is Null");
		return false;
	}

	return hdd_tsf_is_tx_set(hdd_ctx);
}
#endif

/**
 * cds_get_log_indicator() - Get the log flush indicator
 *
 * This function is used to get the log flush indicator
 *
 * Return: log indicator
 */
uint32_t cds_get_log_indicator(void)
{
	struct cds_context *p_cds_context;
	uint32_t indicator;

	p_cds_context = cds_get_global_context();
	if (!p_cds_context) {
		cds_err("cds context is Invalid");
		return WLAN_LOG_INDICATOR_UNUSED;
	}

	if (cds_is_load_or_unload_in_progress() ||
	    cds_is_driver_recovering() || cds_is_driver_in_bad_state()) {
		return WLAN_LOG_INDICATOR_UNUSED;
	}

	qdf_spinlock_acquire(&p_cds_context->bug_report_lock);
	indicator = p_cds_context->log_complete.indicator;
	qdf_spinlock_release(&p_cds_context->bug_report_lock);
	return indicator;
}

/**
 * cds_wlan_flush_host_logs_for_fatal() - Wrapper to flush host logs
 *
 * This function is used to send signal to the logger thread to
 * flush the host logs.
 *
 * Return: None
 *
 */
void cds_wlan_flush_host_logs_for_fatal(void)
{
	if (cds_is_log_report_in_progress())
		wlan_flush_host_logs_for_fatal();
}

/**
 * cds_flush_logs() - Report fatal event to userspace
 * @is_fatal: Indicates if the event triggering bug report is fatal or not
 * @indicator: Source which trigerred the bug report
 * @reason_code: Reason for triggering bug report
 * @dump_mac_trace: If mac trace are needed in logs.
 * @recovery_needed: If recovery is needed after bug report
 *
 * This function sets the log related params and send the WMI command to the
 * FW to flush its logs. On receiving the flush completion event from the FW
 * the same will be conveyed to userspace
 *
 * Return: 0 on success
 */
QDF_STATUS cds_flush_logs(uint32_t is_fatal,
		uint32_t indicator,
		uint32_t reason_code,
		bool dump_mac_trace,
		bool recovery_needed)
{
	QDF_STATUS status;

	struct cds_context *p_cds_context;

	p_cds_context = cds_get_global_context();
	if (!p_cds_context) {
		cds_err("cds context is Invalid");
		return QDF_STATUS_E_FAILURE;
	}
	if (!p_cds_context->enable_fatal_event) {
		cds_err("Fatal event not enabled");
		return QDF_STATUS_E_FAILURE;
	}
	if (cds_is_load_or_unload_in_progress() ||
	    cds_is_driver_recovering() || cds_is_driver_in_bad_state()) {
		cds_err("un/Load/SSR in progress");
		return QDF_STATUS_E_FAILURE;
	}

	if (cds_is_log_report_in_progress()) {
		cds_err("Bug report already in progress - dropping! type:%d, indicator=%d reason_code=%d",
			is_fatal, indicator, reason_code);
		return QDF_STATUS_E_FAILURE;
	}

	status = cds_set_log_completion(is_fatal, indicator,
		reason_code, recovery_needed);
	if (QDF_STATUS_SUCCESS != status) {
		cds_err("Failed to set log trigger params");
		return QDF_STATUS_E_FAILURE;
	}

	cds_debug("Triggering bug report: type:%d, indicator=%d reason_code=%d",
		  is_fatal, indicator, reason_code);

	if (dump_mac_trace)
		qdf_trace_dump_all(p_cds_context->mac_context, 0, 0, 100, 0);

	if (WLAN_LOG_INDICATOR_HOST_ONLY == indicator) {
		cds_wlan_flush_host_logs_for_fatal();
		return QDF_STATUS_SUCCESS;
	}

	status = sme_send_flush_logs_cmd_to_fw();
	if (QDF_IS_STATUS_ERROR(status)) {
		cds_err("Failed to send flush FW log");
		cds_init_log_completion();
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * cds_logging_set_fw_flush_complete() - Wrapper for FW log flush completion
 *
 * This function is used to send signal to the logger thread to indicate
 * that the flushing of FW logs is complete by the FW
 *
 * Return: None
 *
 */
void cds_logging_set_fw_flush_complete(void)
{
	if (cds_is_fatal_event_enabled())
		wlan_logging_set_fw_flush_complete();
}

/**
 * cds_set_fatal_event() - set fatal event status
 * @value: pending statue to set
 *
 * Return: None
 */
void cds_set_fatal_event(bool value)
{
	struct cds_context *p_cds_context;

	p_cds_context = cds_get_global_context();
	if (!p_cds_context) {
		cds_err("cds context is Invalid");
		return;
	}
	p_cds_context->enable_fatal_event = value;
}

/**
 * cds_get_radio_index() - get radio index
 *
 * Return: radio index otherwise, -EINVAL
 */
int cds_get_radio_index(void)
{
	struct cds_context *p_cds_context;

	p_cds_context = cds_get_global_context();
	if (!p_cds_context) {
		/*
		 * To avoid recursive call, this should not change to
		 * QDF_TRACE().
		 */
		pr_err("%s: cds context is invalid\n", __func__);
		return -EINVAL;
	}

	return p_cds_context->radio_index;
}

/**
 * cds_set_radio_index() - set radio index
 * @radio_index:	the radio index to set
 *
 * Return: QDF status
 */
QDF_STATUS cds_set_radio_index(int radio_index)
{
	struct cds_context *p_cds_context;

	p_cds_context = cds_get_global_context();
	if (!p_cds_context) {
		pr_err("%s: cds context is invalid\n", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	p_cds_context->radio_index = radio_index;

	return QDF_STATUS_SUCCESS;
}

/**
 * cds_init_ini_config() - API to initialize CDS configuration parameters
 * @cfg: CDS Configuration
 *
 * Return: void
 */

void cds_init_ini_config(struct cds_config_info *cfg)
{
	struct cds_context *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return;
	}

	cds_ctx->cds_cfg = cfg;
}

/**
 * cds_deinit_ini_config() - API to free CDS configuration parameters
 *
 * Return: void
 */
void cds_deinit_ini_config(void)
{
	struct cds_context *cds_ctx;
	struct cds_config_info *cds_cfg;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return;
	}

	cds_cfg = cds_ctx->cds_cfg;
	cds_ctx->cds_cfg = NULL;

	if (cds_cfg)
		qdf_mem_free(cds_cfg);
}

/**
 * cds_get_ini_config() - API to get CDS configuration parameters
 *
 * Return: cds config structure
 */
struct cds_config_info *cds_get_ini_config(void)
{
	struct cds_context *cds_ctx;

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		cds_err("Invalid CDS Context");
		return NULL;
	}

	return cds_ctx->cds_cfg;
}

/**
 * cds_is_5_mhz_enabled() - API to get 5MHZ enabled
 *
 * Return: true if 5 mhz is enabled, false otherwise
 */
bool cds_is_5_mhz_enabled(void)
{
	struct cds_context *p_cds_context;

	p_cds_context = cds_get_context(QDF_MODULE_ID_QDF);
	if (!p_cds_context) {
		cds_err("cds context is invalid");
		return false;
	}

	if (p_cds_context->cds_cfg)
		return (p_cds_context->cds_cfg->sub_20_channel_width ==
						WLAN_SUB_20_CH_WIDTH_5);

	return false;
}

/**
 * cds_is_10_mhz_enabled() - API to get 10-MHZ enabled
 *
 * Return: true if 10 mhz is enabled, false otherwise
 */
bool cds_is_10_mhz_enabled(void)
{
	struct cds_context *p_cds_context;

	p_cds_context = cds_get_context(QDF_MODULE_ID_QDF);
	if (!p_cds_context) {
		cds_err("cds context is invalid");
		return false;
	}

	if (p_cds_context->cds_cfg)
		return (p_cds_context->cds_cfg->sub_20_channel_width ==
						WLAN_SUB_20_CH_WIDTH_10);

	return false;
}

/**
 * cds_is_sub_20_mhz_enabled() - API to get sub 20-MHZ enabled
 *
 * Return: true if 5 or 10 mhz is enabled, false otherwise
 */
bool cds_is_sub_20_mhz_enabled(void)
{
	struct cds_context *p_cds_context;

	p_cds_context = cds_get_context(QDF_MODULE_ID_QDF);
	if (!p_cds_context) {
		cds_err("cds context is invalid");
		return false;
	}

	if (p_cds_context->cds_cfg)
		return p_cds_context->cds_cfg->sub_20_channel_width;

	return false;
}

/**
 * cds_is_self_recovery_enabled() - API to get self recovery enabled
 *
 * Return: true if self recovery enabled, false otherwise
 */
bool cds_is_self_recovery_enabled(void)
{
	struct cds_context *p_cds_context;

	p_cds_context = cds_get_context(QDF_MODULE_ID_QDF);
	if (!p_cds_context) {
		cds_err("cds context is invalid");
		return false;
	}

	if (p_cds_context->cds_cfg)
		return p_cds_context->cds_cfg->self_recovery_enabled;

	return false;
}

/**
 * cds_is_fw_down() - Is FW down or not
 *
 * Return: true if FW is down and false otherwise.
 */
bool cds_is_fw_down(void)
{
	qdf_device_t qdf_ctx;

	qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);
	if (!qdf_ctx) {
		cds_err("cds context is invalid");
		return false;
	}

	return pld_is_fw_down(qdf_ctx->dev);
}

/**
 * cds_svc_fw_shutdown_ind() - API to send userspace about FW crash
 *
 * @dev: Device Pointer
 *
 * Return: None
 */
void cds_svc_fw_shutdown_ind(struct device *dev)
{
	hdd_svc_fw_shutdown_ind(dev);
}

#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
/*
 * cds_pkt_stats_to_logger_thread() - send pktstats to user
 * @pl_hdr: Pointer to pl_hdr
 * @pkt_dump: Pointer to pkt_dump data structure.
 * @data: Pointer to data
 *
 * This function is used to send the pkt stats to SVC module.
 *
 * Return: None
 */
inline void cds_pkt_stats_to_logger_thread(void *pl_hdr, void *pkt_dump,
						void *data)
{
	if (cds_get_ring_log_level(RING_ID_PER_PACKET_STATS) !=
						WLAN_LOG_LEVEL_ACTIVE)
		return;

	wlan_pkt_stats_to_logger_thread(pl_hdr, pkt_dump, data);
}
#endif

/**
 * cds_get_conparam() - Get the connection mode parameters
 *
 * Return the connection mode parameter set by insmod or set during statically
 * linked driver
 *
 * Return: enum QDF_GLOBAL_MODE
 */
enum QDF_GLOBAL_MODE cds_get_conparam(void)
{
	enum QDF_GLOBAL_MODE con_mode;

	con_mode = hdd_get_conparam();

	return con_mode;
}

#ifdef FEATURE_HTC_CREDIT_HISTORY
inline void
cds_print_htc_credit_history(uint32_t count, qdf_abstract_print *print,
			     void *print_priv)
{
	htc_print_credit_history(gp_cds_context->htc_ctx, count,
				 print, print_priv);
}
#endif

uint32_t cds_get_connectivity_stats_pkt_bitmap(void *context)
{
	struct hdd_adapter *adapter = NULL;

	if (!context)
		return 0;

	adapter = (struct hdd_adapter *)context;
	if (unlikely(adapter->magic != WLAN_HDD_ADAPTER_MAGIC)) {
		cds_err("Magic cookie(%x) for adapter sanity verification is invalid",
			adapter->magic);
		return 0;
	}
	return adapter->pkt_type_bitmap;
}

#ifdef FEATURE_ALIGN_STATS_FROM_DP
/**
 * cds_get_cdp_vdev_stats() - Function which retrieves cdp vdev stats
 * @vdev_id: vdev id
 * @vdev_stats: cdp vdev stats retrieves from DP
 *
 * Return: If get cdp vdev stats success return true, otherwise return false
 */
static bool
cds_get_cdp_vdev_stats(uint8_t vdev_id, struct cdp_vdev_stats *vdev_stats)
{
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);

	if (!vdev_stats)
		return false;

	if (cdp_host_get_vdev_stats(soc, vdev_id, vdev_stats, true))
		return false;

	return true;
}

bool
cds_dp_get_vdev_stats(uint8_t vdev_id, struct cds_vdev_dp_stats *stats)
{
	struct cdp_vdev_stats *vdev_stats;
	bool ret = false;

	vdev_stats = qdf_mem_malloc(sizeof(*vdev_stats));
	if (!vdev_stats)
		return false;

	if (cds_get_cdp_vdev_stats(vdev_id, vdev_stats)) {
		stats->tx_retries = vdev_stats->tx.retries;
		stats->tx_mpdu_success_with_retries =
			vdev_stats->tx.mpdu_success_with_retries;
		ret = true;
	}

	qdf_mem_free(vdev_stats);
	return ret;
}
#endif

#ifdef ENABLE_SMMU_S1_TRANSLATION
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
QDF_STATUS cds_smmu_mem_map_setup(qdf_device_t osdev, bool ipa_present)
{
	struct iommu_domain *domain;
	bool ipa_smmu_enabled;
	bool wlan_smmu_enabled;

	domain = pld_smmu_get_domain(osdev->dev);
	if (domain) {
		int attr = 0;
		int errno = iommu_domain_get_attr(domain,
						  DOMAIN_ATTR_S1_BYPASS, &attr);

		wlan_smmu_enabled = !errno && !attr;
	} else {
		cds_info("No SMMU mapping present");
		wlan_smmu_enabled = false;
	}

	if (!wlan_smmu_enabled) {
		osdev->smmu_s1_enabled = false;
		goto exit_with_success;
	}

	if (!ipa_present) {
		osdev->smmu_s1_enabled = true;
		goto exit_with_success;
	}

	ipa_smmu_enabled = qdf_get_ipa_smmu_enabled();

	osdev->smmu_s1_enabled = ipa_smmu_enabled && wlan_smmu_enabled;
	if (ipa_smmu_enabled != wlan_smmu_enabled) {
		cds_err("SMMU mismatch; IPA:%s, WLAN:%s",
			ipa_smmu_enabled ? "enabled" : "disabled",
			wlan_smmu_enabled ? "enabled" : "disabled");
		return QDF_STATUS_E_FAILURE;
	}

exit_with_success:
	osdev->domain = domain;

	cds_info("SMMU S1 %s", osdev->smmu_s1_enabled ? "enabled" : "disabled");

	return QDF_STATUS_SUCCESS;
}

#else
QDF_STATUS cds_smmu_mem_map_setup(qdf_device_t osdev, bool ipa_present)
{
	struct dma_iommu_mapping *mapping;
	bool ipa_smmu_enabled;
	bool wlan_smmu_enabled;

	mapping = pld_smmu_get_mapping(osdev->dev);
	if (mapping) {
		int attr = 0;
		int errno = iommu_domain_get_attr(mapping->domain,
						  DOMAIN_ATTR_S1_BYPASS, &attr);

		wlan_smmu_enabled = !errno && !attr;
	} else {
		cds_info("No SMMU mapping present");
		wlan_smmu_enabled = false;
	}

	if (!wlan_smmu_enabled) {
		osdev->smmu_s1_enabled = false;
		goto exit_with_success;
	}

	if (!ipa_present) {
		osdev->smmu_s1_enabled = true;
		goto exit_with_success;
	}

	ipa_smmu_enabled = qdf_get_ipa_smmu_enabled();

	osdev->smmu_s1_enabled = ipa_smmu_enabled && wlan_smmu_enabled;
	if (ipa_smmu_enabled != wlan_smmu_enabled) {
		cds_err("SMMU mismatch; IPA:%s, WLAN:%s",
			ipa_smmu_enabled ? "enabled" : "disabled",
			wlan_smmu_enabled ? "enabled" : "disabled");
		return QDF_STATUS_E_FAILURE;
	}

exit_with_success:
	osdev->iommu_mapping = mapping;

	cds_info("SMMU S1 %s", osdev->smmu_s1_enabled ? "enabled" : "disabled");

	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef IPA_OFFLOAD
int cds_smmu_map_unmap(bool map, uint32_t num_buf, qdf_mem_info_t *buf_arr)
{
	return ucfg_ipa_uc_smmu_map(map, num_buf, buf_arr);
}
#else
int cds_smmu_map_unmap(bool map, uint32_t num_buf, qdf_mem_info_t *buf_arr)
{
	return 0;
}
#endif

#else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
QDF_STATUS cds_smmu_mem_map_setup(qdf_device_t osdev, bool ipa_present)
{
	osdev->smmu_s1_enabled = false;
	osdev->domain = NULL;
	return QDF_STATUS_SUCCESS;
}
#else
QDF_STATUS cds_smmu_mem_map_setup(qdf_device_t osdev, bool ipa_present)
{
	osdev->smmu_s1_enabled = false;
	return QDF_STATUS_SUCCESS;
}
#endif

int cds_smmu_map_unmap(bool map, uint32_t num_buf, qdf_mem_info_t *buf_arr)
{
	return 0;
}
#endif
