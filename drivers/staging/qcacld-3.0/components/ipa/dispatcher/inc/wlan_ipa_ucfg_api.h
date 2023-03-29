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
 * DOC: Declare public API related to the wlan ipa called by north bound
 */

#ifndef _WLAN_IPA_UCFG_API_H_
#define _WLAN_IPA_UCFG_API_H_

#include "wlan_ipa_public_struct.h"
#include "wlan_ipa_obj_mgmt_api.h"
#include "wlan_objmgr_pdev_obj.h"
#include "qdf_types.h"
#include "wlan_ipa_main.h"

#ifdef IPA_OFFLOAD

/**
 * ucfg_ipa_is_present() - get IPA hw status
 *
 * ipa_uc_reg_rdyCB is not directly designed to check
 * ipa hw status. This is an undocumented function which
 * has confirmed with IPA team.
 *
 * Return: true - ipa hw present
 *         false - ipa hw not present
 */
bool ucfg_ipa_is_present(void);

/**
 * ucfg_ipa_is_ready() - get IPA ready status
 *
 * After ipa_ready_cb() is registered and later invoked by IPA
 * driver, ipa ready status flag is updated in wlan driver.
 * Unless IPA ready callback is invoked and ready status is
 * updated none of the IPA APIs should be invoked.
 *
 * Return: true - ipa is ready
 *         false - ipa is not ready
 */
bool ucfg_ipa_is_ready(void);

/**
 * ucfg_ipa_is_enabled() - get IPA enable status
 *
 * Return: true - ipa is enabled
 *         false - ipa is not enabled
 */
bool ucfg_ipa_is_enabled(void);

/**
 * ucfg_ipa_uc_is_enabled() - get IPA uC enable status
 *
 * Return: true - ipa uC is enabled
 *         false - ipa uC is not enabled
 */
bool ucfg_ipa_uc_is_enabled(void);

/**
 * ucfg_ipa_set_dp_handle() - register DP handle
 * @psoc: psoc handle
 * @dp_soc: data path soc handle
 *
 * Return: None
 */
void ucfg_ipa_set_dp_handle(struct wlan_objmgr_psoc *psoc,
			       void *dp_soc);

/**
 * ucfg_ipa_set_pdev_id() - register pdev id
 * @psoc: psoc handle
 * @pdev_id: data path txrx pdev id
 *
 * Return: None
 */
void ucfg_ipa_set_pdev_id(struct wlan_objmgr_psoc *psoc,
			  uint8_t pdev_id);

/**
 * ucfg_ipa_set_perf_level() - Set IPA perf level
 * @pdev: pdev obj
 * @tx_packets: Number of packets transmitted in the last sample period
 * @rx_packets: Number of packets received in the last sample period
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ucfg_ipa_set_perf_level(struct wlan_objmgr_pdev *pdev,
				   uint64_t tx_packets, uint64_t rx_packets);

/**
 * ucfg_ipa_uc_info() - Print IPA uC resource and session information
 * @pdev: pdev obj
 *
 * Return: None
 */
void ucfg_ipa_uc_info(struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_ipa_uc_stat() - Print IPA uC stats
 * @pdev: pdev obj
 *
 * Return: None
 */
void ucfg_ipa_uc_stat(struct wlan_objmgr_pdev *pdev);


/**
 * ucfg_ipa_uc_rt_debug_host_dump() - IPA rt debug host dump
 * @pdev: pdev obj
 *
 * Return: None
 */
void ucfg_ipa_uc_rt_debug_host_dump(struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_ipa_dump_info() - Dump IPA context information
 * @pdev: pdev obj
 *
 * Return: None
 */
void ucfg_ipa_dump_info(struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_ipa_uc_stat_request() - Get IPA stats from IPA.
 * @pdev: pdev obj
 * @reason: STAT REQ Reason
 *
 * Return: None
 */
void ucfg_ipa_uc_stat_request(struct wlan_objmgr_pdev *pdev,
			      uint8_t reason);

/**
 * ucfg_ipa_uc_stat_query() - Query the IPA stats
 * @pdev: pdev obj
 * @ipa_tx_diff: tx packet count diff from previous tx packet count
 * @ipa_rx_diff: rx packet count diff from previous rx packet count
 *
 * Return: None
 */
void ucfg_ipa_uc_stat_query(struct wlan_objmgr_pdev *pdev,
			    uint32_t *ipa_tx_diff, uint32_t *ipa_rx_diff);

/**
 * ucfg_ipa_reg_sap_xmit_cb() - Register upper layer SAP cb to transmit
 * @pdev: pdev obj
 * @cb: callback
 *
 * Return: None
 */
void ucfg_ipa_reg_sap_xmit_cb(struct wlan_objmgr_pdev *pdev,
			      wlan_ipa_softap_xmit cb);

/**
 * ucfg_ipa_reg_send_to_nw_cb() - Register cb to send IPA Rx packet to network
 * @pdev: pdev obj
 * @cb: callback
 *
 * Return: None
 */
void ucfg_ipa_reg_send_to_nw_cb(struct wlan_objmgr_pdev *pdev,
				wlan_ipa_send_to_nw cb);

/**
 * ucfg_ipa_reg_rps_enable_cb() - Register cb to enable RPS
 * @pdev: pdev obj
 * @cb: callback
 *
 * Return: None
 */
#ifdef IPA_LAN_RX_NAPI_SUPPORT
void ucfg_ipa_reg_rps_enable_cb(struct wlan_objmgr_pdev *pdev,
				wlan_ipa_rps_enable cb);
#else
static inline
void ucfg_ipa_reg_rps_enable_cb(struct wlan_objmgr_pdev *pdev,
				wlan_ipa_rps_enable cb)
{
}
#endif

/**
 * ucfg_ipa_set_mcc_mode() - Set MCC mode
 * @pdev: pdev obj
 * @mcc_mode: 0=MCC/1=SCC
 *
 * Return: void
 */
void ucfg_ipa_set_mcc_mode(struct wlan_objmgr_pdev *pdev, bool mcc_mode);

/**
 * ucfg_ipa_set_dfs_cac_tx() - Set DFS cac tx block
 * @pdev: pdev obj
 * @tx_block: dfs cac tx block
 *
 * Return: void
 */
void ucfg_ipa_set_dfs_cac_tx(struct wlan_objmgr_pdev *pdev, bool tx_block);

/**
 * ucfg_ipa_set_ap_ibss_fwd() - Set AP intra bss forward
 * @pdev: pdev obj
 * @session_id: vdev id
 * @intra_bss: enable or disable ap intra bss forward
 *
 * Return: void
 */
void ucfg_ipa_set_ap_ibss_fwd(struct wlan_objmgr_pdev *pdev, uint8_t session_id,
			      bool intra_bss);

/**
 * ucfg_ipa_uc_force_pipe_shutdown() - Force shutdown IPA pipe
 * @pdev: pdev obj
 *
 * Return: void
 */
void ucfg_ipa_uc_force_pipe_shutdown(struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_ipa_flush() - flush IPA exception path SKB's
 * @pdev: pdev obj
 *
 * Return: None
 */
void ucfg_ipa_flush(struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_ipa_suspend() - Suspend IPA
 * @pdev: pdev obj
 *
 * Return: QDF STATUS
 */
QDF_STATUS ucfg_ipa_suspend(struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_ipa_resume() - Resume IPA
 * @pdev: pdev obj
 *
 * Return: QDF STATUS
 */
QDF_STATUS ucfg_ipa_resume(struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_ipa_uc_ol_init() - Initialize IPA uC offload
 * @pdev: pdev obj
 * @osdev: OS dev
 *
 * Return: QDF STATUS
 */
QDF_STATUS ucfg_ipa_uc_ol_init(struct wlan_objmgr_pdev *pdev,
			       qdf_device_t osdev);

/**
 * ucfg_ipa_uc_ol_deinit() - Deinitialize IPA uC offload
 * @pdev: pdev obj
 *
 * Return: QDF STATUS
 */
QDF_STATUS ucfg_ipa_uc_ol_deinit(struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_ipa_is_tx_pending() - Check if IPA WLAN TX completions are pending
 * @pdev: pdev obj
 *
 * Return: bool if pending TX for IPA.
 */
bool ucfg_ipa_is_tx_pending(struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_ipa_send_mcc_scc_msg() - Send IPA WLAN_SWITCH_TO_MCC/SCC message
 * @mcc_mode: 0=MCC/1=SCC
 *
 * Return: QDF STATUS
 */
QDF_STATUS ucfg_ipa_send_mcc_scc_msg(struct wlan_objmgr_pdev *pdev,
				     bool mcc_mode);

/**
 * ucfg_ipa_wlan_evt() - IPA event handler
 * @pdev: pdev obj
 * @net_dev: Interface net device
 * @device_mode: Net interface device mode
 * @session_id: session id for the event
 * @type: event enum of type ipa_wlan_event
 * @mac_address: MAC address associated with the event
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_ipa_wlan_evt(struct wlan_objmgr_pdev *pdev,
			     qdf_netdev_t net_dev, uint8_t device_mode,
			     uint8_t session_id,
			     enum wlan_ipa_wlan_event ipa_event_type,
			     uint8_t *mac_addr);

/**
 * ucfg_ipa_uc_smmu_map() - Map / Unmap DMA buffer to IPA UC
 * @map: Map / unmap operation
 * @num_buf: Number of buffers in array
 * @buf_arr: Buffer array of DMA mem mapping info
 *
 * Return: Status of map operation
 */
int ucfg_ipa_uc_smmu_map(bool map, uint32_t num_buf, qdf_mem_info_t *buf_arr);

/**
 * ucfg_ipa_is_fw_wdi_activated - Is FW WDI activated?
 * @pdev: pdev obj
 *
 * Return: true if FW WDI activated, false otherwise
 */
bool ucfg_ipa_is_fw_wdi_activated(struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_ipa_uc_cleanup_sta() - disconnect and cleanup sta iface
 * @pdev: pdev obj
 * @net_dev: Interface net device
 *
 * Send disconnect sta event to IPA driver and cleanup IPA iface,
 * if not yet done
 *
 * Return: void
 */
void ucfg_ipa_uc_cleanup_sta(struct wlan_objmgr_pdev *pdev,
			     qdf_netdev_t net_dev);

/**
 * ucfg_ipa_uc_disconnect_ap() - send ap disconnect event
 * @pdev: pdev obj
 * @net_dev: Interface net device
 *
 * Send disconnect ap event to IPA driver during SSR
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_ipa_uc_disconnect_ap(struct wlan_objmgr_pdev *pdev,
				     qdf_netdev_t net_dev);

/**
 * ucfg_ipa_cleanup_dev_iface() - Clean up net dev IPA interface
 * @pdev: pdev obj
 * @net_dev: Interface net device
 *
 *
 * Return: None
 */
void ucfg_ipa_cleanup_dev_iface(struct wlan_objmgr_pdev *pdev,
				qdf_netdev_t net_dev);

/**
 * ucfg_ipa_uc_ssr_cleanup() - Handle IPA cleanup for SSR
 * @pdev: pdev obj
 *
 * From hostside do cleanup such as deregister IPA interafces
 * and send disconnect events so that it will be sync after SSR
 *
 * Return: None
 */
void ucfg_ipa_uc_ssr_cleanup(struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_ipa_fw_rejuvenate_send_msg() - Send msg to IPA driver in FW rejuvenate
 * @pdev: pdev obj
 *
 * Return: None
 */
void ucfg_ipa_fw_rejuvenate_send_msg(struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_ipa_component_config_update() - update IPA component config
 * @psoc: pointer to psoc object
 *
 * Return: None
 */
void ucfg_ipa_component_config_update(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_ipa_component_config_free() - Free IPA component config
 *
 * Return: None
 */
void ucfg_ipa_component_config_free(void);

/**
 * ucfg_get_ipa_tx_buf_count() - get IPA tx buffer count
 *
 * Return: IPA tx buffer count
 */
uint32_t ucfg_ipa_get_tx_buf_count(void);

/**
 * ucfg_ipa_update_tx_stats() - send embedded tx traffic in bytes to IPA
 * @pdev: pdev obj
 * @sta_tx: tx in bytes on sta vdev
 * @ap_tx: tx in bytes on sap vdev
 *
 * Return: void
 */
void ucfg_ipa_update_tx_stats(struct wlan_objmgr_pdev *pdev, uint64_t sta_tx,
			      uint64_t ap_tx);
/**
 * ucfg_ipa_flush_pending_vdev_events() - flush pending vdev wlan ipa events
 * @pdev: pdev obj
 * @vdev_id: vdev id
 *
 * Return: None
 */
void ucfg_ipa_flush_pending_vdev_events(struct wlan_objmgr_pdev *pdev,
					uint8_t vdev_id);
#else

static inline bool ucfg_ipa_is_present(void)
{
	return false;
}

static inline bool ucfg_ipa_is_ready(void)
{
	return false;
}

static inline void ucfg_ipa_update_config(struct wlan_ipa_config *config)
{
}

static inline bool ucfg_ipa_is_enabled(void)
{
	return false;
}

static inline bool ucfg_ipa_uc_is_enabled(void)
{
	return false;
}

static inline
QDF_STATUS ucfg_ipa_set_dp_handle(struct wlan_objmgr_psoc *psoc,
				     void *dp_soc)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS ucfg_ipa_set_pdev_id(struct wlan_objmgr_psoc *psoc,
				uint8_t pdev_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS ucfg_ipa_set_perf_level(struct wlan_objmgr_pdev *pdev,
				   uint64_t tx_packets, uint64_t rx_packets)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void ucfg_ipa_uc_info(struct wlan_objmgr_pdev *pdev)
{
}

static inline
void ucfg_ipa_uc_stat(struct wlan_objmgr_pdev *pdev)
{
}

static inline
void ucfg_ipa_uc_rt_debug_host_dump(struct wlan_objmgr_pdev *pdev)
{
}

static inline
void ucfg_ipa_dump_info(struct wlan_objmgr_pdev *pdev)
{
}

static inline
void ucfg_ipa_uc_stat_request(struct wlan_objmgr_pdev *pdev,
			      uint8_t reason)
{
}

static inline
void ucfg_ipa_uc_stat_query(struct wlan_objmgr_pdev *pdev,
			    uint32_t *ipa_tx_diff, uint32_t *ipa_rx_diff)
{
}

static inline
void ucfg_ipa_reg_sap_xmit_cb(struct wlan_objmgr_pdev *pdev,
			      wlan_ipa_softap_xmit cb)
{
}

static inline
void ucfg_ipa_reg_send_to_nw_cb(struct wlan_objmgr_pdev *pdev,
				wlan_ipa_send_to_nw cb)
{
}

static inline
void ucfg_ipa_reg_rps_enable_cb(struct wlan_objmgr_pdev *pdev,
				wlan_ipa_rps_enable cb)
{
}

static inline
void ucfg_ipa_set_mcc_mode(struct wlan_objmgr_pdev *pdev, bool mcc_mode)
{
}

static inline
void ucfg_ipa_set_dfs_cac_tx(struct wlan_objmgr_pdev *pdev, bool tx_block)
{
}

static inline
void ucfg_ipa_set_ap_ibss_fwd(struct wlan_objmgr_pdev *pdev, uint8_t session_id,
			      bool intra_bss)
{
}

static inline
void ucfg_ipa_uc_force_pipe_shutdown(struct wlan_objmgr_pdev *pdev)
{
}

static inline
void ucfg_ipa_flush(struct wlan_objmgr_pdev *pdev)
{
}

static inline
QDF_STATUS ucfg_ipa_suspend(struct wlan_objmgr_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS ucfg_ipa_resume(struct wlan_objmgr_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS ucfg_ipa_uc_ol_init(struct wlan_objmgr_pdev *pdev,
			       qdf_device_t osdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS ucfg_ipa_uc_ol_deinit(struct wlan_objmgr_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline bool ucfg_ipa_is_tx_pending(struct wlan_objmgr_pdev *pdev)
{
	return false;
}

static inline
QDF_STATUS ucfg_ipa_send_mcc_scc_msg(struct wlan_objmgr_pdev *pdev,
				     bool mcc_mode)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS ucfg_ipa_wlan_evt(struct wlan_objmgr_pdev *pdev,
			     qdf_netdev_t net_dev, uint8_t device_mode,
			     uint8_t session_id,
			     enum wlan_ipa_wlan_event ipa_event_type,
			     uint8_t *mac_addr)
{
	return QDF_STATUS_SUCCESS;
}

static inline
int ucfg_ipa_uc_smmu_map(bool map, uint32_t num_buf, qdf_mem_info_t *buf_arr)
{
	return 0;
}

static inline
bool ucfg_ipa_is_fw_wdi_activated(struct wlan_objmgr_pdev *pdev)
{
	return false;
}

static inline
void ucfg_ipa_uc_cleanup_sta(struct wlan_objmgr_pdev *pdev,
			     qdf_netdev_t net_dev)
{
}

static inline
QDF_STATUS ucfg_ipa_uc_disconnect_ap(struct wlan_objmgr_pdev *pdev,
				     qdf_netdev_t net_dev)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void ucfg_ipa_cleanup_dev_iface(struct wlan_objmgr_pdev *pdev,
				qdf_netdev_t net_dev)
{
}

static inline
void ucfg_ipa_uc_ssr_cleanup(struct wlan_objmgr_pdev *pdev)
{
}

static inline
void ucfg_ipa_fw_rejuvenate_send_msg(struct wlan_objmgr_pdev *pdev)
{
}

static inline
void ucfg_ipa_component_config_update(struct wlan_objmgr_psoc *psoc)
{
}

static inline
void ucfg_ipa_component_config_free(void)
{
}

static inline
uint32_t ucfg_ipa_get_tx_buf_count(void)
{
	return 0;
}

static inline
void ucfg_ipa_update_tx_stats(struct wlan_objmgr_pdev *pdev, uint64_t sta_tx,
			      uint64_t ap_tx)
{
}

static inline
void ucfg_ipa_flush_pending_vdev_events(struct wlan_objmgr_pdev *pdev,
					uint8_t vdev_id)
{
}
#endif /* IPA_OFFLOAD */
#endif /* _WLAN_IPA_UCFG_API_H_ */
