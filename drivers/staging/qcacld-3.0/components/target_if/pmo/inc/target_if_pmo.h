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
 * DOC: Declare various api/struct which shall be used
 * by pmo component for wmi cmd (tx path) and
 * event (rx) handling.
 */

#ifndef _TARGET_IF_PMO_H_
#define _TARGET_IF_PMO_H_

#include "target_if.h"
#include "wlan_pmo_tgt_api.h"
#include "wlan_pmo_obj_mgmt_public_struct.h"

/**
 * target_if_pmo_enable_wow_wakeup_event() - Enable wow wakeup events.
 * @vdev:objmgr vdev handle
 * @bitmap: Event bitmap
 * @enable: enable/disable
 *
 * Return: QDF status
 */
QDF_STATUS target_if_pmo_enable_wow_wakeup_event(struct wlan_objmgr_vdev *vdev,
		uint32_t *bitmap);

/**
 * target_if_pmo_disable_wow_wakeup_event() -  Disable wow wakeup events.
 * @vdev:objmgr vdev handle
 * @bitmap: Event bitmap
 * @enable: enable/disable
 *
 * Return: QDF status
 */
QDF_STATUS target_if_pmo_disable_wow_wakeup_event(
		struct wlan_objmgr_vdev *vdev, uint32_t *bitmap);

/**
 * target_if_pmo_send_wow_patterns_to_fw() - Sends WOW patterns to FW.
 * @vdev: objmgr vdev handle
 * @ptrn_id: pattern id
 * @ptrn: pattern
 * @ptrn_len: pattern length
 * @ptrn_offset: pattern offset
 * @mask: mask
 * @mask_len: mask length
 * @user: true for user configured pattern and false for default pattern
 *
 * Return: QDF status
 */
QDF_STATUS target_if_pmo_send_wow_patterns_to_fw(struct wlan_objmgr_vdev *vdev,
		uint8_t ptrn_id,
		const uint8_t *ptrn, uint8_t ptrn_len,
		uint8_t ptrn_offset, const uint8_t *mask,
		uint8_t mask_len, bool user);

QDF_STATUS target_if_pmo_del_wow_patterns_to_fw(struct wlan_objmgr_vdev *vdev,
		uint8_t ptrn_id);

/**
 * target_if_pmo_send_enhance_mc_offload_req() - send enhance mc offload req
 * @vdev: objmgr vdev
 * @action: enable or disable enhance multicast offload
 *
 * Return: QDF_STATUS_SUCCESS on success else error code
 */
QDF_STATUS target_if_pmo_send_enhance_mc_offload_req(
		struct wlan_objmgr_vdev *vdev,
		bool enable);

/**
 * target_if_pmo_set_mc_filter_req() - set mcast filter command to fw
 * @vdev: objmgr vdev handle
 * @multicastAddr: mcast address
 *
 * Return: 0 for success or error code
 */
QDF_STATUS target_if_pmo_set_mc_filter_req(struct wlan_objmgr_vdev *vdev,
		struct qdf_mac_addr multicast_addr);

/**
 * target_if_pmo_clear_mc_filter_req() - clear mcast filter command to fw
 * @vdev: objmgr vdev handle
 * @multicastAddr: mcast address
 *
 * Return: 0 for success or error code
 */
QDF_STATUS target_if_pmo_clear_mc_filter_req(struct wlan_objmgr_vdev *vdev,
		struct qdf_mac_addr multicast_addr);

/**
 * target_if_pmo_get_multiple_mc_filter_support() - get multiple mc filter
 *						    request fw support
 * @psoc: the psoc containing the vdev to configure
 *
 * Return: true if fw supports else false
 */
bool target_if_pmo_get_multiple_mc_filter_support(
		struct wlan_objmgr_psoc *psoc);

/**
 * target_if_pmo_set_multiple_mc_filter_req() - set multiple mcast filter
 *						command to fw
 * @vdev: objmgr vdev handle
 * @multicastAddr: mcast address
 *
 * Return: 0 for success or error code
 */
QDF_STATUS target_if_pmo_set_multiple_mc_filter_req(
		struct wlan_objmgr_vdev *vdev,
		struct pmo_mc_addr_list *mc_list);

/**
 * target_if_pmo_clear_multiple_mc_filter_req() - clear multiple mcast
 *						  filter command to fw
 * @vdev: objmgr vdev handle
 * @multicastAddr: mcast address
 *
 * Return: 0 for success or error code
 */
QDF_STATUS target_if_pmo_clear_multiple_mc_filter_req(
		struct wlan_objmgr_vdev *vdev,
		struct pmo_mc_addr_list *mc_list);

/**
 * target_if_pmo_send_ra_filter_req() - set RA filter pattern in fw
 * @vdev: objmgr vdev handle
 * @default_pattern: default pattern id
 * @rate_limit_interval: ra rate limit interval
 *
 * Return: QDF status
 */
QDF_STATUS target_if_pmo_send_ra_filter_req(struct wlan_objmgr_vdev *vdev,
		uint8_t default_pattern, uint16_t rate_limit_interval);

/**
 * target_if_pmo_send_action_frame_patterns() - register action frame map to fw
 * @handle: Pointer to wma handle
 * @vdev_id: VDEV ID
 *
 * This is called to push action frames wow patterns from local
 * cache to firmware.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS target_if_pmo_send_action_frame_patterns(
		struct wlan_objmgr_vdev *vdev,
		struct pmo_action_wakeup_set_params *ip_cmd);

/**
 * target_if_pmo_conf_hw_filter() - configure hardware filter in DTIM mode
 * @psoc: the psoc containing the vdev to configure
 * @req: the request parameters
 *
 * Return: QDF_STATUS
 */
QDF_STATUS target_if_pmo_conf_hw_filter(struct wlan_objmgr_psoc *psoc,
					struct pmo_hw_filter_params *req);

#ifdef WLAN_FEATURE_PACKET_FILTERING
/**
 * target_if_pmo_send_pkt_filter_req() - enable packet filter
 * @vdev: objmgr vdev
 * @rcv_filter_param: filter params
 *
 * This function enable packet filter
 *
 *  Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS target_if_pmo_send_pkt_filter_req(struct wlan_objmgr_vdev *vdev,
			struct pmo_rcv_pkt_fltr_cfg *rcv_filter_param);

/**
 * target_if_pmo_clear_pkt_filter_req() - disable packet filter
 * @vdev: objmgr vdev
 * @rcv_clear_param: filter params
 *
 * This function disable packet filter
 *
 *  Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS target_if_pmo_clear_pkt_filter_req(struct wlan_objmgr_vdev *vdev,
			struct pmo_rcv_pkt_fltr_clear_param *rcv_clear_param);
#endif

/**
 * target_if_pmo_send_arp_offload_req() - sends arp request to fwr
 * @vdev: objmgr vdev
 * @arp_offload_req: arp offload req
 * @ns_offload_req: ns offload request
 *
 * This functions sends arp request to fwr.
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS target_if_pmo_send_arp_offload_req(
		struct wlan_objmgr_vdev *vdev,
		struct pmo_arp_offload_params *arp_offload_req,
		struct pmo_ns_offload_params *ns_offload_req);

#ifdef WLAN_NS_OFFLOAD
/**
 * target_if_pmo_send_ns_offload_req() - sends ns request to fwr
 * @vdev: objmgr vdev
 * @arp_offload_req: arp offload req
 * @ns_offload_req: ns offload request
 *
 * This functions sends ns request to fwr.
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS target_if_pmo_send_ns_offload_req(
		struct wlan_objmgr_vdev *vdev,
		struct pmo_arp_offload_params *arp_offload_req,
		struct pmo_ns_offload_params *ns_offload_req);
#else /* WLAN_NS_OFFLOAD */
static inline QDF_STATUS
target_if_pmo_send_ns_offload_req(struct wlan_objmgr_vdev *vdev,
		struct pmo_arp_offload_params *arp_offload_req,
		struct pmo_ns_offload_params *ns_offload_req)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_NS_OFFLOAD */
/**
 * target_if_pmo_send_gtk_offload_req() - send gtk offload request in fwr
 * @vdev: objmgr vdev handle
 * @gtk_offload_req: gtk offload request
 *
 * Return: QDF status
 */
QDF_STATUS target_if_pmo_send_gtk_offload_req(struct wlan_objmgr_vdev *vdev,
		struct pmo_gtk_req *gtk_offload_req);

/**
 * target_if_pmo_send_gtk_response_req() - send gtk response request in fwr
 * @vdev: objmgr vdev handle
 *
 * Return: QDF status
 */
QDF_STATUS target_if_pmo_send_gtk_response_req(struct wlan_objmgr_vdev *vdev);

/**
 * target_if_pmo_gtk_offload_status_event() - GTK offload status event handler
 * @scn_handle: scn handle
 * @event: event buffer
 * @len: buffer length
 *
 * Return: 0 for success or error code
 */
int target_if_pmo_gtk_offload_status_event(void *scn_handle,
	uint8_t *event, uint32_t len);

/**
 * target_if_pmo_send_lphb_enable() - enable command of LPHB config req
 * @psoc: objmgr psoc handle
 * @ts_lphb_enable: lphb enable request which needs to configure in fwr
 *
 * Return: QDF status
 */
QDF_STATUS target_if_pmo_send_lphb_enable(struct wlan_objmgr_psoc *psoc,
			struct pmo_lphb_enable_req *ts_lphb_enable);

/**
 * target_if_pmo_send_lphb_tcp_params() - set lphb tcp params config request
 * @psoc: objmgr psoc handle
 * @ts_lphb_tcp_param: lphb tcp params which needs to configure in fwr
 *
 * Return: QDF status
 */
QDF_STATUS target_if_pmo_send_lphb_tcp_params(struct wlan_objmgr_psoc *psoc,
			struct pmo_lphb_tcp_params *ts_lphb_tcp_param);

/**
 * target_if_pmo_send_lphb_tcp_pkt_filter() - send lphb tcp packet filter req
 * @psoc: objmgr psoc handle
 * @ts_lphb_tcp_filter: lphb tcp filter request which needs to configure in fwr
 *
 * Return: QDF status
 */
QDF_STATUS target_if_pmo_send_lphb_tcp_pkt_filter(struct wlan_objmgr_psoc *psoc,
			struct pmo_lphb_tcp_filter_req *ts_lphb_tcp_filter);

/**
 * target_if_pmo_send_lphb_udp_params() - Send udp param command of LPHB
 * @psoc: objmgr psoc handle
 * @ts_lphb_udp_param: lphb udp params which needs to configure in fwr
 *
 * Return: QDF status
 */
QDF_STATUS target_if_pmo_send_lphb_udp_params(struct wlan_objmgr_psoc *psoc,
			struct pmo_lphb_udp_params *ts_lphb_udp_param);

/**
 * target_if_pmo_send_lphb_udp_pkt_filter() - Send lphb udp pkt filter cmd req
 * @psoc: objmgr psoc handle
 * @ts_lphb_udp_filter: lphb udp filter request which needs to configure in fwr
 *
 * Return: QDF status
 */
QDF_STATUS target_if_pmo_send_lphb_udp_pkt_filter(struct wlan_objmgr_psoc *psoc,
			struct pmo_lphb_udp_filter_req *ts_lphb_udp_filter);

/**
 * target_if_pmo_lphb_evt_handler() - send LPHB indication to os if /HDD
 * @psoc: objmgr psoc handle
 * @event: lphb event buffer
 *
 * Return: QDF_STATUS_SUCCESS for success else error code
 */
QDF_STATUS target_if_pmo_lphb_evt_handler(struct wlan_objmgr_psoc *psoc,
		uint8_t *event);

/**
 * target_if_pmo_send_vdev_update_param_req() - Send vdev param value to fwr
 * @vdev: objmgr vdev
 * @param_id: tell vdev param id which needs to be updated in fwr
 * @param_value: vdev parameter value
 *
 * Return: QDF status
 */
QDF_STATUS target_if_pmo_send_vdev_update_param_req(
		struct wlan_objmgr_vdev *vdev,
		uint32_t param_id, uint32_t param_value);

/**
 * target_if_pmo_send_vdev_ps_param_req() - Send vdev ps param value to fwr
 * @vdev: objmgr vdev
 * @param_id: tell vdev param id which needs to be updated in fwr
 * @param_value: vdev parameter value
 *
 * Return: QDF status
 */
QDF_STATUS target_if_pmo_send_vdev_ps_param_req(
		struct wlan_objmgr_vdev *vdev,
		uint32_t param_id,
		uint32_t param_value);

#ifdef WLAN_FEATURE_IGMP_OFFLOAD
/**
 * target_if_pmo_send_igmp_offload_req() - Send igmp offload req to fw
 * @vdev: objmgr vdev
 * @pmo_igmp_req: igmp req
 *
 * Return: QDF status
 */
QDF_STATUS
target_if_pmo_send_igmp_offload_req(
		struct wlan_objmgr_vdev *vdev,
		struct pmo_igmp_offload_req *pmo_igmp_req);
#else
static inline QDF_STATUS
target_if_pmo_send_igmp_offload_req(
		struct wlan_objmgr_vdev *vdev,
		struct pmo_igmp_offload_req *pmo_igmp_req)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * target_if_pmo_psoc_update_bus_suspend() - update wmi bus suspend flag
 * @psoc: objmgr psoc
 * @value: bus suspend value
 *
 * Return: None
 */
void target_if_pmo_psoc_update_bus_suspend(struct wlan_objmgr_psoc *psoc,
		uint8_t value);

/**
 * target_if_pmo_psoc_get_host_credits() - get available host credits
 * @psoc: objmgr psoc
 *
 * Return: return host credits
 */
int target_if_pmo_psoc_get_host_credits(struct wlan_objmgr_psoc *psoc);

/**
 * target_if_pmo_psoc_get_pending_cmnds() - get wmi pending commands
 * @psoc: objmgr psoc
 *
 * Return: return wmi pending commands
 */
int target_if_pmo_psoc_get_pending_cmnds(struct wlan_objmgr_psoc *psoc);

/**
 * target_if_pmo_update_target_suspend_flag() - set wmi target suspend flag
 * @psoc: objmgr psoc
 * @value: value
 *
 * Return: None
 */
void target_if_pmo_update_target_suspend_flag(struct wlan_objmgr_psoc *psoc,
		uint8_t value);

/**
 * target_if_pmo_update_target_suspend_acked_flag() - set wmi target suspend
 *                                                    acked flag
 * @psoc: objmgr psoc
 * @value: value
 *
 * Return: None
 */
void target_if_pmo_update_target_suspend_acked_flag(
						struct wlan_objmgr_psoc *psoc,
						uint8_t value);

/**
 * target_if_pmo_is_target_suspended() - get wmi target suspend flag
 * @psoc: objmgr psoc
 *
 * Return: true if target suspended, false otherwise
 */
bool target_if_pmo_is_target_suspended(struct wlan_objmgr_psoc *psoc);

/**
 * target_if_pmo_psoc_send_wow_enable_req() -send wow enable request
 * @psoc: objmgr psoc
 * @param: wow command params
 *
 * Return: return QDF_STATUS_SUCCESS on success else error code
 */
QDF_STATUS target_if_pmo_psoc_send_wow_enable_req(struct wlan_objmgr_psoc *psoc,
		struct pmo_wow_cmd_params *param);

/**
 * target_if_pmo_psoc_send_suspend_req() - fp to send suspend request
 * @psoc: objmgr psoc
 * @param: target suspend params
 *
 * Return: return QDF_STATUS_SUCCESS on success else error code
 */
QDF_STATUS target_if_pmo_psoc_send_suspend_req(struct wlan_objmgr_psoc *psoc,
		struct pmo_suspend_params *param);

/**
 * target_if_pmo_set_runtime_pm_in_progress() - set runtime pm status
 * @psoc: objmgr psoc
 * @value: set runtime pm status
 *
 * Return: none
 */
void target_if_pmo_set_runtime_pm_in_progress(struct wlan_objmgr_psoc *psoc,
					      bool value);

/**
 * target_if_pmo_get_runtime_pm_in_progress() - fp to get runtime pm status
 * @psoc: objmgr psoc
 *
 * Return: true if runtime pm in progress else false
 */
bool target_if_pmo_get_runtime_pm_in_progress(struct wlan_objmgr_psoc *psoc);

/**
 * target_if_pmo_psoc_send_host_wakeup_ind() - send host wake ind to fwr
 * @psoc: objmgr psoc
 *
 * Return: return QDF_STATUS_SUCCESS on success else error code
 */
QDF_STATUS target_if_pmo_psoc_send_host_wakeup_ind(
		struct wlan_objmgr_psoc *psoc);

/**
 * target_if_pmo_psoc_send_target_resume_req() -send target resume request
 * @psoc: objmgr psoc
 *
 * Return: return QDF_STATUS_SUCCESS on success else error code
 */
QDF_STATUS target_if_pmo_psoc_send_target_resume_req(
		struct wlan_objmgr_psoc *psoc);

/**
 * target_if_pmo_psoc_send_d0wow_enable_req() - send d0 wow enable request
 * @psoc: objmgr psoc
 *
 * Return: return QDF_STATUS_SUCCESS on success else error code
 */
QDF_STATUS target_if_pmo_psoc_send_d0wow_enable_req(
		struct wlan_objmgr_psoc *psoc);

/**
 * target_if_pmo_psoc_send_d0wow_disable_req() - send d0 wow disable request
 * @psoc: objmgr psoc
 *
 * Return: return QDF_STATUS_SUCCESS on success else error code
 */
QDF_STATUS target_if_pmo_psoc_send_d0wow_disable_req(
		struct wlan_objmgr_psoc *psoc);

/**
 * target_if_pmo_psoc_send_idle_monitor_cmd() - send screen status to firmware
 * @psoc: objmgr psoc
 * @val: Idle monitor value
 *
 * Return: QDF_STATUS_SUCCESS on success else error code
 */
QDF_STATUS
target_if_pmo_psoc_send_idle_monitor_cmd(struct wlan_objmgr_psoc *psoc,
					 uint8_t val);

/**
 * target_if_pmo_register_tx_ops() - Register PMO component TX OPS
 * @tx_ops: PMO if transmit ops
 *
 * Return: None
 */
void target_if_pmo_register_tx_ops(struct wlan_pmo_tx_ops *tx_ops);

#endif

