/*
 * Copyright (c) 2013-2021 The Linux Foundation. All rights reserved.
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

#ifndef _WLAN_IPA_CORE_H_
#define _WLAN_IPA_CORE_H_

#ifdef IPA_OFFLOAD

#include "wlan_ipa_priv.h"
#include "wlan_ipa_public_struct.h"

/**
 * wlan_ipa_is_enabled() - Is IPA enabled?
 * @ipa_cfg: IPA config
 *
 * Return: true if IPA is enabled, false otherwise
 */
static inline bool wlan_ipa_is_enabled(struct wlan_ipa_config *ipa_cfg)
{
	return WLAN_IPA_IS_CONFIG_ENABLED(ipa_cfg, WLAN_IPA_ENABLE_MASK);
}

/**
 * wlan_ipa_uc_is_enabled() - Is IPA UC enabled?
 * @ipa_cfg: IPA config
 *
 * Return: true if IPA UC is enabled, false otherwise
 */
static inline bool wlan_ipa_uc_is_enabled(struct wlan_ipa_config *ipa_cfg)
{
	return WLAN_IPA_IS_CONFIG_ENABLED(ipa_cfg, WLAN_IPA_UC_ENABLE_MASK);
}

/**
 * wlan_ipa_is_rt_debugging_enabled() - Is IPA RT debugging enabled?
 * @ipa_cfg: IPA config
 *
 * Return: true if IPA RT debugging is enabled, false otherwise
 */
static inline
bool wlan_ipa_is_rt_debugging_enabled(struct wlan_ipa_config *ipa_cfg)
{
	return WLAN_IPA_IS_CONFIG_ENABLED(ipa_cfg,
					  WLAN_IPA_REAL_TIME_DEBUGGING);
}

/**
 * wlan_ipa_setup - IPA initialize and setup
 * @ipa_ctx: IPA priv obj
 * @ipa_cfg: IPA config
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_ipa_setup(struct wlan_ipa_priv *ipa_ctx,
			  struct wlan_ipa_config *ipa_cfg);

/**
 * wlan_ipa_get_obj_context - Get IPA OBJ context
 *
 * Return: IPA context
 */
struct wlan_ipa_priv *wlan_ipa_get_obj_context(void);

/**
 * wlan_ipa_cleanup - IPA cleanup
 * @ipa_ctx: IPA priv obj
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_ipa_cleanup(struct wlan_ipa_priv *ipa_ctx);

/**
 * wlan_ipa_uc_enable_pipes() - Enable IPA uC pipes
 * @ipa_ctx: IPA context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_ipa_uc_enable_pipes(struct wlan_ipa_priv *ipa_ctx);

/**
 * wlan_ipa_uc_disable_pipes() - Disable IPA uC pipes
 * @ipa_ctx: IPA context
 * @force_disable: If true, immediately disable IPA pipes. If false, wait for
 *		   pending IPA WLAN TX completions
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_ipa_uc_disable_pipes(struct wlan_ipa_priv *ipa_ctx,
				     bool force_disable);

/**
 * wlan_ipa_is_tx_pending() - Check if IPA TX Completions are pending
 * @ipa_ctx: IPA context
 *
 * Return: bool
 */
bool wlan_ipa_is_tx_pending(struct wlan_ipa_priv *ipa_ctx);

/**
 * wlan_ipa_set_perf_level() - Set IPA performance level
 * @ipa_ctx: IPA context
 * @tx_packets: Number of packets transmitted in the last sample period
 * @rx_packets: Number of packets received in the last sample period
 *
 * Return: QDF STATUS
 */
QDF_STATUS wlan_ipa_set_perf_level(struct wlan_ipa_priv *ipa_ctx,
				   uint64_t tx_packets, uint64_t rx_packets);

/**
 * wlan_ipa_init_perf_level() - Initialize IPA performance level
 * @ipa_ctx: IPA context
 *
 * If IPA clock scaling is disabled, initialize perf level to maximum.
 * Else set the lowest level to start with.
 *
 * Return: QDF STATUS
 */
QDF_STATUS wlan_ipa_init_perf_level(struct wlan_ipa_priv *ipa_ctx);

/**
 * wlan_ipa_get_iface() - Get IPA interface
 * @ipa_ctx: IPA context
 * @mode: Interface device mode
 *
 * Return: IPA interface address
 */
struct wlan_ipa_iface_context
*wlan_ipa_get_iface(struct wlan_ipa_priv *ipa_ctx, uint8_t mode);

/**
 * wlan_ipa_get_iface_by_mode_netdev() - Get IPA interface
 * @ipa_ctx: IPA context
 * @ndev: Interface netdev pointer
 * @mode: Interface device mode
 *
 * Return: IPA interface address
 */
struct wlan_ipa_iface_context *
wlan_ipa_get_iface_by_mode_netdev(struct wlan_ipa_priv *ipa_ctx,
				  qdf_netdev_t ndev, uint8_t mode);

#ifndef CONFIG_IPA_WDI_UNIFIED_API

/**
 * wlan_ipa_is_rm_enabled() - Is IPA RM enabled?
 * @ipa_cfg: IPA config
 *
 * Return: true if IPA RM is enabled, false otherwise
 */
static inline bool wlan_ipa_is_rm_enabled(struct wlan_ipa_config *ipa_cfg)
{
	return WLAN_IPA_IS_CONFIG_ENABLED(ipa_cfg, WLAN_IPA_RM_ENABLE_MASK);
}

/**
 * wlan_ipa_is_clk_scaling_enabled() - Is IPA clock scaling enabled?
 * @ipa_cfg: IPA config
 *
 * Return: true if IPA clock scaling is enabled, false otherwise
 */
static inline
bool wlan_ipa_is_clk_scaling_enabled(struct wlan_ipa_config *ipa_cfg)
{
	return WLAN_IPA_IS_CONFIG_ENABLED(ipa_cfg,
					  WLAN_IPA_CLK_SCALING_ENABLE_MASK |
					  WLAN_IPA_RM_ENABLE_MASK);
}

/**
 * wlan_ipa_wdi_rm_request_resource() - IPA WDI request resource
 * @ipa_ctx: IPA context
 * @res_name: IPA RM resource name
 *
 * Return: 0 on success, negative errno on error
 */
static inline
int wlan_ipa_wdi_rm_request_resource(struct wlan_ipa_priv *ipa_ctx,
				     qdf_ipa_rm_resource_name_t res_name)
{
	return qdf_ipa_rm_request_resource(res_name);
}

/**
 * wlan_ipa_wdi_rm_release_resource() - IPA WDI release resource
 * @ipa_ctx: IPA context
 * @res_name: IPA RM resource name
 *
 * Return: 0 on success, negative errno on error
 */
static inline
int wlan_ipa_wdi_rm_release_resource(struct wlan_ipa_priv *ipa_ctx,
				     qdf_ipa_rm_resource_name_t res_name)
{
	return qdf_ipa_rm_release_resource(res_name);
}

/**
 * wlan_ipa_wdi_rm_request() - Request resource from IPA
 * @ipa_ctx: IPA context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_ipa_wdi_rm_request(struct wlan_ipa_priv *ipa_ctx);

/**
 * wlan_ipa_wdi_rm_try_release() - Attempt to release IPA resource
 * @ipa_ctx: IPA context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_ipa_wdi_rm_try_release(struct wlan_ipa_priv *ipa_ctx);

/**
 * wlan_ipa_wdi_setup_rm() - Setup IPA resource management
 * @ipa_ctx: IPA context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_ipa_wdi_setup_rm(struct wlan_ipa_priv *ipa_ctx);

/**
 * wlan_ipa_wdi_destroy_rm() - Destroy IPA resources
 * @ipa_ctx: IPA context
 *
 * Destroys all resources associated with the IPA resource manager
 *
 * Return: None
 */
void wlan_ipa_wdi_destroy_rm(struct wlan_ipa_priv *ipa_ctx);

static inline
int wlan_ipa_wdi_rm_notify_completion(qdf_ipa_rm_event_t event,
				      qdf_ipa_rm_resource_name_t res_name)
{
	return qdf_ipa_rm_notify_completion(event, res_name);
}

static inline
int wlan_ipa_wdi_rm_inactivity_timer_destroy(
					qdf_ipa_rm_resource_name_t res_name)
{
	return qdf_ipa_rm_inactivity_timer_destroy(res_name);
}

bool wlan_ipa_is_rm_released(struct wlan_ipa_priv *ipa_ctx);

#else /* CONFIG_IPA_WDI_UNIFIED_API */

/**
 * wlan_ipa_is_rm_enabled() - Is IPA RM enabled?
 * @ipa_cfg: IPA config
 *
 * IPA RM is deprecated and IPA PM is involved. WLAN driver
 * has no control over IPA PM and thus we could regard IPA
 * RM as always enabled for power efficiency.
 *
 * Return: true
 */
static inline bool wlan_ipa_is_rm_enabled(struct wlan_ipa_config *ipa_cfg)
{
	return true;
}

/**
 * wlan_ipa_is_clk_scaling_enabled() - Is IPA clock scaling enabled?
 * @ipa_cfg: IPA config
 *
 * Return: true if IPA clock scaling is enabled, false otherwise
 */
static inline
bool wlan_ipa_is_clk_scaling_enabled(struct wlan_ipa_config *ipa_cfg)
{
	return WLAN_IPA_IS_CONFIG_ENABLED(ipa_cfg,
					  WLAN_IPA_CLK_SCALING_ENABLE_MASK);
}

static inline int wlan_ipa_wdi_rm_request_resource(
			struct wlan_ipa_priv *ipa_ctx,
			qdf_ipa_rm_resource_name_t res_name)
{
	return 0;
}

static inline int wlan_ipa_wdi_rm_release_resource(
			struct wlan_ipa_priv *ipa_ctx,
			qdf_ipa_rm_resource_name_t res_name)
{
	return 0;
}

static inline QDF_STATUS wlan_ipa_wdi_setup_rm(struct wlan_ipa_priv *ipa_ctx)
{
	return 0;
}

static inline int wlan_ipa_wdi_destroy_rm(struct wlan_ipa_priv *ipa_ctx)
{
	return 0;
}

static inline QDF_STATUS wlan_ipa_wdi_rm_request(struct wlan_ipa_priv *ipa_ctx)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS wlan_ipa_wdi_rm_try_release(struct wlan_ipa_priv
						     *ipa_ctx)
{
	return QDF_STATUS_SUCCESS;
}

static inline
int wlan_ipa_wdi_rm_notify_completion(qdf_ipa_rm_event_t event,
				      qdf_ipa_rm_resource_name_t res_name)
{
	return 0;
}

static inline
int wlan_ipa_wdi_rm_inactivity_timer_destroy(
					qdf_ipa_rm_resource_name_t res_name)
{
	return 0;
}

static inline
bool wlan_ipa_is_rm_released(struct wlan_ipa_priv *ipa_ctx)
{
	return true;
}

#endif /* CONFIG_IPA_WDI_UNIFIED_API */

#ifdef FEATURE_METERING

#ifndef WDI3_STATS_UPDATE
/**
 * wlan_ipa_uc_op_metering() - IPA uC operation for stats and quota limit
 * @ipa_ctx: IPA context
 * @op_msg: operation message received from firmware
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS wlan_ipa_uc_op_metering(struct wlan_ipa_priv *ipa_ctx,
				   struct op_msg_type *op_msg);
#else
static inline
QDF_STATUS wlan_ipa_uc_op_metering(struct wlan_ipa_priv *ipa_ctx,
				   struct op_msg_type *op_msg)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * wlan_ipa_wdi_meter_notifier_cb() - SSR wrapper for
 * __wlan_ipa_wdi_meter_notifier_cb
 * @priv: pointer to private data registered with IPA (we register a
 *        pointer to the global IPA context)
 * @evt: the IPA event which triggered the callback
 * @data: data associated with the event
 *
 * Return: None
 */
void wlan_ipa_wdi_meter_notifier_cb(qdf_ipa_wdi_meter_evt_type_t evt,
				    void *data);

/**
 * wlan_ipa_init_metering() - IPA metering stats completion event reset
 * @ipa_ctx: IPA context
 *
 * Return: QDF_STATUS enumeration
 */
void wlan_ipa_init_metering(struct wlan_ipa_priv *ipa_ctx);

#ifdef WDI3_STATS_UPDATE
/**
 * wlan_ipa_update_tx_stats() - send embedded tx traffic in bytes to IPA
 * @ipa_ctx: IPA context
 * @sta_tx: tx in bytes on sta interface
 * @sap_tx: tx in bytes on sap interface
 *
 * Return: void
 */
void wlan_ipa_update_tx_stats(struct wlan_ipa_priv *ipa_ctx, uint64_t sta_tx,
			      uint64_t sap_tx);
#else
static inline void wlan_ipa_update_tx_stats(struct wlan_ipa_priv *ipa_ctx,
					    uint64_t sta_tx, uint64_t sap_tx)
{
}
#endif /* WDI3_STATS_UPDATE */

#else

static inline
QDF_STATUS wlan_ipa_uc_op_metering(struct wlan_ipa_priv *ipa_ctx,
				   struct op_msg_type *op_msg)
{
	return QDF_STATUS_SUCCESS;
}

static inline void wlan_ipa_wdi_meter_notifier_cb(void)
{
}

static inline void wlan_ipa_init_metering(struct wlan_ipa_priv *ipa_ctx)
{
}

static inline void wlan_ipa_update_tx_stats(struct wlan_ipa_priv *ipa_ctx,
					    uint64_t sta_tx, uint64_t sap_tx)
{
}
#endif /* FEATURE_METERING */

/**
 * wlan_ipa_uc_stat() - Print IPA uC stats
 * @ipa_ctx: IPA context
 *
 * Return: None
 */
void wlan_ipa_uc_stat(struct wlan_ipa_priv *ipa_ctx);

/**
 * wlan_ipa_uc_info() - Print IPA uC resource and session information
 * @ipa_ctx: IPA context
 *
 * Return: None
 */
void wlan_ipa_uc_info(struct wlan_ipa_priv *ipa_ctx);

/**
 * wlan_ipa_print_fw_wdi_stats() - Print FW IPA WDI stats
 * @ipa_ctx: IPA context
 *
 * Return: None
 */
void wlan_ipa_print_fw_wdi_stats(struct wlan_ipa_priv *ipa_ctx,
				 struct ipa_uc_fw_stats *uc_fw_stat);

/**
 * wlan_ipa_uc_stat_request() - Get IPA stats from IPA
 * @ipa_ctx: IPA context
 * @reason: STAT REQ Reason
 *
 * Return: None
 */
void wlan_ipa_uc_stat_request(struct wlan_ipa_priv *ipa_ctx, uint8_t reason);

/**
 * wlan_ipa_uc_stat_query() - Query the IPA stats
 * @ipa_ctx: IPA context
 * @ipa_tx_diff: tx packet count diff from previous tx packet count
 * @ipa_rx_diff: rx packet count diff from previous rx packet count
 *
 * Return: None
 */
void wlan_ipa_uc_stat_query(struct wlan_ipa_priv *ipa_ctx,
			    uint32_t *ipa_tx_diff, uint32_t *ipa_rx_diff);

/**
 * wlan_ipa_dump_info() - dump IPA IPA struct
 * @ipa_ctx: IPA context
 *
 * Dump entire struct ipa_ctx
 *
 * Return: none
 */
void wlan_ipa_dump_info(struct wlan_ipa_priv *ipa_ctx);

/**
 * wlan_ipa_uc_rt_debug_host_dump - dump rt debug buffer
 * @ipa_ctx: IPA context
 *
 * If rt debug enabled, dump debug buffer contents based on requirement
 *
 * Return: none
 */
void wlan_ipa_uc_rt_debug_host_dump(struct wlan_ipa_priv *ipa_ctx);

/**
 * wlan_ipa_uc_rt_debug_destructor() - called by data packet free
 * @nbuff: packet pinter
 *
 * when free data packet, will be invoked by wlan client and will increase
 * free counter
 *
 * Return: none
 */
void wlan_ipa_uc_rt_debug_destructor(qdf_nbuf_t nbuff);

/**
 * wlan_ipa_uc_rt_debug_deinit() - remove resources to handle rt debugging
 * @ipa_ctx: IPA context
 *
 * free all rt debugging resources
 *
 * Return: none
 */
void wlan_ipa_uc_rt_debug_deinit(struct wlan_ipa_priv *ipa_ctx);

/**
 * wlan_ipa_uc_rt_debug_init() - initialize resources to handle rt debugging
 * @ipa_ctx: IPA context
 *
 * alloc and initialize all rt debugging resources
 *
 * Return: none
 */
void wlan_ipa_uc_rt_debug_init(struct wlan_ipa_priv *ipa_ctx);

/**
 * wlan_ipa_reg_sap_xmit_cb() - Register upper layer SAP cb to transmit
 * @ipa_ctx: IPA context
 * @cb: callback
 *
 * Return: None
 */
static inline
void wlan_ipa_reg_sap_xmit_cb(struct wlan_ipa_priv *ipa_ctx,
			      wlan_ipa_softap_xmit cb)
{
	ipa_ctx->softap_xmit = cb;
}

/**
 * wlan_ipa_reg_send_to_nw_cb() - Register cb to send IPA Rx packet to network
 * @ipa_ctx: IPA context
 * @cb: callback
 *
 * Return: None
 */
static inline
void wlan_ipa_reg_send_to_nw_cb(struct wlan_ipa_priv *ipa_ctx,
				wlan_ipa_send_to_nw cb)
{
	ipa_ctx->send_to_nw = cb;
}

#ifdef IPA_LAN_RX_NAPI_SUPPORT
/**
 * wlan_ipa_reg_rps_enable_cb() - Register callback to enable RPS
 * @ipa_ctx: IPA context
 * @cb: callback
 *
 * Return: None
 */
static inline
void wlan_ipa_reg_rps_enable_cb(struct wlan_ipa_priv *ipa_ctx,
				wlan_ipa_rps_enable cb)
{
	ipa_ctx->rps_enable = cb;
}

/**
 * ipa_set_rps_enable(): Enable/disable RPS for all interfaces of specific mode
 * @ipa_ctx: IPA context
 * @mode: mode of interface for which RPS needs to be enabled
 * @enable: Set true to enable RPS
 *
 * Return: None
 */
void ipa_set_rps(struct wlan_ipa_priv *ipa_ctx, enum QDF_OPMODE mode,
		 bool enable);

/**
 * ipa_set_rps_per_vdev(): Enable/disable RPS for a specific vdev
 * @ipa_ctx: IPA context
 * @vdev_id: vdev id for which RPS needs to be enabled
 * @enable: Set true to enable RPS
 *
 * Return: None
 */
static inline
void ipa_set_rps_per_vdev(struct wlan_ipa_priv *ipa_ctx, uint8_t vdev_id,
			  bool enable)
{
	if (ipa_ctx->rps_enable)
		ipa_ctx->rps_enable(vdev_id, enable);
}

/**
 * wlan_ipa_handle_multiple_sap_evt() - Handle multiple SAP connect/disconnect
 * @ipa_ctx: IPA global context
 * @type: IPA event type.
 *
 * This function is used to disable pipes when multiple SAP are connected and
 * enable pipes back when only one SAP is connected.
 *
 * Return: None
 */
void wlan_ipa_handle_multiple_sap_evt(struct wlan_ipa_priv *ipa_ctx,
				      qdf_ipa_wlan_event type);

#else
static inline
void ipa_set_rps(struct wlan_ipa_priv *ipa_ctx, enum QDF_OPMODE mode,
		 bool enable)
{
}

static inline
void ipa_set_rps_per_vdev(struct wlan_ipa_priv *ipa_ctx, uint8_t vdev_id,
			  bool enable)
{
}

static inline
void wlan_ipa_handle_multiple_sap_evt(struct wlan_ipa_priv *ipa_ctx,
				      qdf_ipa_wlan_event type)
{
}

#endif

/**
 * wlan_ipa_set_mcc_mode() - Set MCC mode
 * @ipa_ctx: IPA context
 * @mcc_mode: 1=MCC/0=SCC
 *
 * Return: void
 */
void wlan_ipa_set_mcc_mode(struct wlan_ipa_priv *ipa_ctx, bool mcc_mode);

/**
 * wlan_ipa_set_dfs_cac_tx() - Set DFS cac tx block
 * @ipa_ctx: IPA context
 * @tx_block: dfs cac tx block
 *
 * Return: void
 */
static inline
void wlan_ipa_set_dfs_cac_tx(struct wlan_ipa_priv *ipa_ctx, bool tx_block)
{
	ipa_ctx->dfs_cac_block_tx = tx_block;
}

/**
 * wlan_ipa_set_ap_ibss_fwd() - Set AP intra bss forward
 * @ipa_ctx: IPA context
 * @session_id: vdev id
 * @intra_bss: 1 to disable ap intra bss forward and 0 to enable ap intra bss
 *	       forward
 *
 * Return: void
 */
static inline
void wlan_ipa_set_ap_ibss_fwd(struct wlan_ipa_priv *ipa_ctx, uint8_t session_id,
			      bool intra_bss)
{
	if (session_id >= WLAN_IPA_MAX_SESSION)
		return;

	ipa_ctx->disable_intrabss_fwd[session_id] = intra_bss;
}

/**
 * wlan_ipa_uc_ol_init() - Initialize IPA uC offload
 * @ipa_ctx: IPA context
 * @osdev: Parent device instance
 *
 * This function is called to update IPA pipe configuration with resources
 * allocated by wlan driver (cds_pre_enable) before enabling it in FW
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_ipa_uc_ol_init(struct wlan_ipa_priv *ipa_ctx,
			       qdf_device_t osdev);

/**
 * wlan_ipa_uc_ol_deinit() - Disconnect IPA TX and RX pipes
 * @ipa_ctx: IPA context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_ipa_uc_ol_deinit(struct wlan_ipa_priv *ipa_ctx);

/**
 * wlan_ipa_flush() - flush IPA exception path SKB's
 * @ipa_ctx: IPA context
 *
 * Return: None
 */
void wlan_ipa_flush(struct wlan_ipa_priv *ipa_ctx);

/**
 * wlan_ipa_suspend() - Suspend IPA
 * @ipa_ctx: IPA context
 *
 * Return: QDF STATUS
 */
QDF_STATUS wlan_ipa_suspend(struct wlan_ipa_priv *ipa_ctx);

/**
 * wlan_ipa_resume() - Resume IPA
 * @ipa_ctx: IPA context
 *
 * Return: QDF STATUS
 */
QDF_STATUS wlan_ipa_resume(struct wlan_ipa_priv *ipa_ctx);

#ifndef QCA_LL_TX_FLOW_CONTROL_V2
/**
 * wlan_ipa_send_mcc_scc_msg() - Send IPA WLAN_SWITCH_TO_MCC/SCC message
 * @ipa_ctx: IPA context
 * @mcc_mode: 0=MCC/1=SCC
 *
 * Return: QDF STATUS
 */
QDF_STATUS wlan_ipa_send_mcc_scc_msg(struct wlan_ipa_priv *ipa_ctx,
				     bool mcc_mode);
#else
static inline
QDF_STATUS wlan_ipa_send_mcc_scc_msg(struct wlan_ipa_priv *ipa_ctx,
				     bool mcc_mode)
{
	return QDF_STATUS_SUCCESS;
}

static inline void wlan_ipa_mcc_work_handler(void *data)
{
}
#endif

/**
 * wlan_ipa_wlan_evt() - IPA event handler
 * @net_dev: Interface net device
 * @device_mode: Net interface device mode
 * @session_id: session id for the event
 * @type: event enum of type ipa_wlan_event
 * @mac_address: MAC address associated with the event
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_ipa_wlan_evt(qdf_netdev_t net_dev, uint8_t device_mode,
			     uint8_t session_id,
			     enum wlan_ipa_wlan_event ipa_event_type,
			     uint8_t *mac_addr);

/**
 * wlan_ipa_uc_smmu_map() - Map / Unmap DMA buffer to IPA UC
 * @map: Map / unmap operation
 * @num_buf: Number of buffers in array
 * @buf_arr: Buffer array of DMA mem mapping info
 *
 * This API maps/unmaps WLAN-IPA buffers if SMMU S1 translation
 * is enabled.
 *
 * Return: Status of map operation
 */
int wlan_ipa_uc_smmu_map(bool map, uint32_t num_buf, qdf_mem_info_t *buf_arr);

/**
 * wlan_ipa_is_fw_wdi_activated() - Is FW WDI actived?
 * @ipa_ctx: IPA contex
 *
 * Return: true if FW WDI actived, false otherwise
 */
bool wlan_ipa_is_fw_wdi_activated(struct wlan_ipa_priv *ipa_ctx);

/**
 * wlan_ipa_uc_cleanup_sta - disconnect and cleanup sta iface
 * @ipa_ctx: IPA context
 * @net_dev: Interface net device
 *
 * Send disconnect sta event to IPA driver and cleanup IPA iface
 * if not yet done
 *
 * Return: void
 */
void wlan_ipa_uc_cleanup_sta(struct wlan_ipa_priv *ipa_ctx,
			     qdf_netdev_t net_dev);

/**
 * wlan_ipa_uc_disconnect_ap() - send ap disconnect event
 * @ipa_ctx: IPA context
 * @net_dev: Interface net device
 *
 * Send disconnect ap event to IPA driver
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_ipa_uc_disconnect_ap(struct wlan_ipa_priv *ipa_ctx,
				     qdf_netdev_t net_dev);

/**
 * wlan_ipa_cleanup_dev_iface() - Clean up net dev IPA interface
 * @ipa_ctx: IPA context
 * @net_dev: Interface net device
 *
 * Return: None
 */
void wlan_ipa_cleanup_dev_iface(struct wlan_ipa_priv *ipa_ctx,
				qdf_netdev_t net_dev);

/**
 * wlan_ipa_uc_ssr_cleanup() - handle IPA UC clean up during SSR
 * @ipa_ctx: IPA context
 *
 * Return: None
 */
void wlan_ipa_uc_ssr_cleanup(struct wlan_ipa_priv *ipa_ctx);

/**
 * wlan_ipa_fw_rejuvenate_send_msg() - send fw rejuvenate message to IPA driver
 * @ipa_ctx: IPA context
 *
 * Return: void
 */
void wlan_ipa_fw_rejuvenate_send_msg(struct wlan_ipa_priv *ipa_ctx);

/**
 * wlan_ipa_flush_pending_vdev_events() - flush pending vdev ipa events
 * @ipa_ctx: IPA context
 * vdev_id: vdev id
 *
 * This function is to flush vdev wlan ipa pending events
 *
 * Return: None
 */
void wlan_ipa_flush_pending_vdev_events(struct wlan_ipa_priv *ipa_ctx,
					uint8_t vdev_id);
#endif /* IPA_OFFLOAD */
#endif /* _WLAN_IPA_CORE_H_ */
