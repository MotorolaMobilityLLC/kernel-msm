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
 * DOC: Declare public API for pmo to interact with target/WMI
 */

#ifndef _WLAN_PMO_TGT_API_H_
#define _WLAN_PMO_TGT_API_H_

#include "wlan_pmo_common_public_struct.h"
#include "wlan_pmo_arp_public_struct.h"
#include "wlan_pmo_ns_public_struct.h"
#include "wlan_pmo_gtk_public_struct.h"
#include "wlan_pmo_wow_public_struct.h"
#include "wlan_pmo_mc_addr_filtering_public_struct.h"
#include "wlan_pmo_hw_filter_public_struct.h"
#include "wlan_pmo_pkt_filter_public_struct.h"

#define GET_PMO_TX_OPS_FROM_PSOC(psoc) \
	(pmo_psoc_get_priv(psoc)->pmo_tx_ops)

/**
 * pmo_tgt_conf_hw_filter() - configure hardware filter mode in firmware
 * @psoc: the psoc to use to communicate with firmware
 * @req: the configuration request
 *
 * Return: QDF_STATUS
 */
QDF_STATUS pmo_tgt_conf_hw_filter(struct wlan_objmgr_psoc *psoc,
				  struct pmo_hw_filter_params *req);

/**
 * pmo_tgt_set_pkt_filter() - Set packet filter
 * @vdev: objmgr vdev
 * @pmo_set_pkt_fltr_req:
 * @vdev_id: vdev id
 *
 * API to set packet filter
 *
 * Return: QDF_STATUS_SUCCESS in case of success else return error
 */
QDF_STATUS pmo_tgt_set_pkt_filter(struct wlan_objmgr_vdev *vdev,
		struct pmo_rcv_pkt_fltr_cfg *pmo_set_pkt_fltr_req,
		uint8_t vdev_id);

/**
 * pmo_tgt_clear_pkt_filter() - Clear packet filter
 * @vdev: objmgr vdev
 * @pmo_clr_pkt_fltr_param:
 * @vdev_id: vdev id
 *
 * API to clear packet filter
 *
 * Return: QDF_STATUS_SUCCESS in case of success else return error
 */
QDF_STATUS pmo_tgt_clear_pkt_filter(struct wlan_objmgr_vdev *vdev,
		struct pmo_rcv_pkt_fltr_clear_param *pmo_clr_pkt_fltr_param,
		uint8_t vdev_id);

/**
 * pmo_tgt_enable_arp_offload_req() - Enable arp offload req to target
 * @vdev: objmgr vdev
 * @vdev_id: vdev id
 *
 * Return: QDF status
 */
QDF_STATUS pmo_tgt_enable_arp_offload_req(struct wlan_objmgr_vdev *vdev,
		uint8_t vdev_id);

/**
 * pmo_tgt_disable_arp_offload_req() - Disable arp offload req to target
 * @vdev: objmgr vdev
 * @vdev_id: vdev id
 *
 * Return: QDF status
 */
QDF_STATUS pmo_tgt_disable_arp_offload_req(struct wlan_objmgr_vdev *vdev,
		uint8_t vdev_id);

#ifdef WLAN_NS_OFFLOAD
/**
 * pmo_tgt_enable_ns_offload_req() -  Send ns offload req to targe
 * @vdev: objmgr vdev
 * @vdev_id: vdev id
 *
 * Return: QDF status
 */
QDF_STATUS pmo_tgt_enable_ns_offload_req(struct wlan_objmgr_vdev *vdev,
		uint8_t vdev_id);

/**
 * pmo_tgt_disable_ns_offload_req() - Disable arp offload req to target
 * @vdev: objmgr vdev
 * @vdev_id: vdev id
 *
 * Return: QDF status
 */
QDF_STATUS pmo_tgt_disable_ns_offload_req(struct wlan_objmgr_vdev *vdev,
		uint8_t vdev_id);
#endif /* WLAN_NS_OFFLOAD */

/**
 * pmo_tgt_enable_wow_wakeup_event() - Send Enable wow wakeup events req to fwr
 * @vdev: objmgr vdev handle
 * @bitmap: Event bitmap
 *
 * Return: QDF status
 */
QDF_STATUS pmo_tgt_enable_wow_wakeup_event(struct wlan_objmgr_vdev *vdev,
		uint32_t *bitmap);

/**
 * pmo_tgt_disable_wow_wakeup_event() - Send Disable wow wakeup events to fwr
 * @vdev: objmgr vdev handle
 * @bitmap: Event bitmap
 *
 * Return: QDF status
 */
QDF_STATUS pmo_tgt_disable_wow_wakeup_event(struct wlan_objmgr_vdev *vdev,
		uint32_t *bitmap);

/**
 * pmo_tgt_send_wow_patterns_to_fw() - Sends WOW patterns to FW.
 * @vdev: objmgr vdev
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
QDF_STATUS pmo_tgt_send_wow_patterns_to_fw(struct wlan_objmgr_vdev *vdev,
		uint8_t ptrn_id, const uint8_t *ptrn, uint8_t ptrn_len,
		uint8_t ptrn_offset, const uint8_t *mask,
		uint8_t mask_len, bool user);

QDF_STATUS pmo_tgt_del_wow_pattern(
		struct wlan_objmgr_vdev *vdev, uint8_t ptrn_id,
		bool user);

/**
 * pmo_tgt_set_mc_filter_req() - Set mcast filter command to fw
 * @vdev: objmgr vdev
 * @multicastAddr: mcast address
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS pmo_tgt_set_mc_filter_req(struct wlan_objmgr_vdev *vdev,
		struct qdf_mac_addr multicast_addr);

/**
 * pmo_tgt_clear_mc_filter_req() - Clear mcast filter command to fw
 * @vdev: objmgr vdev
 * @multicastAddr: mcast address
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS pmo_tgt_clear_mc_filter_req(struct wlan_objmgr_vdev *vdev,
		struct qdf_mac_addr multicast_addr);

/**
 * pmo_tgt_get_multiple_mc_filter_support() - get multiple mcast filter support
 * @vdev: objmgr vdev
 *
 * Return: true if FW supports else false
 */
bool pmo_tgt_get_multiple_mc_filter_support(struct wlan_objmgr_vdev *vdev);

/**
 * pmo_tgt_set_multiple_mc_filter_req() - Set multiple mcast filter cmd to fw
 * @vdev: objmgr vdev
 * @mc_list: mcast address list
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS pmo_tgt_set_multiple_mc_filter_req(struct wlan_objmgr_vdev *vdev,
		struct pmo_mc_addr_list *mc_list);

/**
 * pmo_tgt_clear_multiple_mc_filter_req() - clear multiple mcast filter
 *					    to fw
 * @vdev: objmgr vdev
 * @mc_list: mcast address list
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS pmo_tgt_clear_multiple_mc_filter_req(struct wlan_objmgr_vdev *vdev,
		struct pmo_mc_addr_list *mc_list);

/**
 * pmo_tgt_send_enhance_multicast_offload_req() - send enhance mc offload req
 * @vdev: the vdev to configure
 * @action: enable or disable enhance multicast offload
 *
 * Return: QDF_STATUS_SUCCESS on success else error code
 */
QDF_STATUS pmo_tgt_send_enhance_multicast_offload_req(
		struct wlan_objmgr_vdev *vdev,
		uint8_t action);

/**
 * pmo_tgt_send_ra_filter_req() - send ra filter request to target
 * @vdev: objmgr vdev handle
 *
 * Return: QDF_STATUS_SUCCESS on success else error code
 */
QDF_STATUS pmo_tgt_send_ra_filter_req(struct wlan_objmgr_vdev *vdev);

/**
 * pmo_tgt_send_action_frame_pattern_req - send wow action frame patterns req
 * @vdev: objmgr vdev handle
 * @cmd: action frame pattern cmd
 *
 * Return: QDF_STATUS_SUCCESS on success else error code
 */
QDF_STATUS pmo_tgt_send_action_frame_pattern_req(
		struct wlan_objmgr_vdev *vdev,
		struct pmo_action_wakeup_set_params *cmd);

/**
 * pmo_tgt_send_gtk_offload_req() - send GTK offload command to fw
 * @vdev: objmgr vdev
 * @gtk_req: pmo gtk req
 *
 * Return: QDF status
 */
QDF_STATUS pmo_tgt_send_gtk_offload_req(struct wlan_objmgr_vdev *vdev,
		struct pmo_gtk_req *gtk_req);

/**
 * pmo_tgt_get_gtk_rsp() - send get gtk rsp command to fw
 * @vdev: objmgr vdev
 *
 * Return: QDF status
 */
QDF_STATUS pmo_tgt_get_gtk_rsp(struct wlan_objmgr_vdev *vdev);

/**
 * pmo_tgt_gtk_rsp_evt() - receive gtk rsp event from fwr
 * @psoc: objmgr psoc
 * @gtk_rsp_param: gtk response parameters
 *
 * Return: QDF status
 */
QDF_STATUS pmo_tgt_gtk_rsp_evt(struct wlan_objmgr_psoc *psoc,
		struct pmo_gtk_rsp_params *rsp_param);

/**
 * pmo_tgt_send_lphb_enable() - enable command of LPHB configuration requests
 * @psoc: objmgr psoc handle
 * @ts_lphb_enable: lphb enable request which needs to configure in fwr
 *
 * Return: QDF status
 */
QDF_STATUS pmo_tgt_send_lphb_enable(struct wlan_objmgr_psoc *psoc,
			struct pmo_lphb_enable_req *ts_lphb_enable);

/**
 * pmo_tgt_send_lphb_tcp_params() - set tcp params of LPHB configuration req
 * @psoc: objmgr psoc handle
 * @ts_lphb_tcp_param: lphb tcp params which needs to configure in fwr
 *
 * Return: QDF status
 */
QDF_STATUS pmo_tgt_send_lphb_tcp_params(struct wlan_objmgr_psoc *psoc,
			struct pmo_lphb_tcp_params *ts_lphb_tcp_param);

/**
 * pmo_tgt_send_lphb_tcp_pkt_filter() - send tcp packet filter command of LPHB
 * @psoc: objmgr psoc handle
 * @ts_lphb_tcp_filter: lphb tcp filter request which needs to configure in fwr
 *
 * Return: QDF status
 */
QDF_STATUS pmo_tgt_send_lphb_tcp_pkt_filter(struct wlan_objmgr_psoc *psoc,
			struct pmo_lphb_tcp_filter_req *ts_lphb_tcp_filter);

/**
 * pmo_tgt_send_lphb_udp_params() - Send udp param command of LPHB
 * @psoc: objmgr psoc handle
 * @ts_lphb_udp_param: lphb udp params which needs to configure in fwr
 *
 * Return: QDF status
 */
QDF_STATUS pmo_tgt_send_lphb_udp_params(struct wlan_objmgr_psoc *psoc,
			struct pmo_lphb_udp_params *ts_lphb_udp_param);

/**
 * pmo_tgt_send_lphb_udp_pkt_filter() - Send udp pkt filter command of LPHB
 * @psoc: objmgr psoc handle
 * @ts_lphb_udp_filter: lphb udp filter request which needs to configure in fwr
 *
 * Return: QDF status
 */
QDF_STATUS pmo_tgt_send_lphb_udp_pkt_filter(struct wlan_objmgr_psoc *psoc,
			struct pmo_lphb_udp_filter_req *ts_lphb_udp_filter);


/**
 * pmo_tgt_lphb_rsp_evt() - receive lphb rsp event from fwr
 * @psoc: objmgr psoc
 * @rsp_param: lphb response parameters
 *
 * Return: QDF_STATUS
 */
QDF_STATUS pmo_tgt_lphb_rsp_evt(struct wlan_objmgr_psoc *psoc,
			struct pmo_lphb_rsp *rsp_param);

/**
 * pmo_tgt_vdev_update_param_req() - Update vdev param value to fwr
 * @vdev: objmgr vdev
 * @param_id: tell vdev param id which needs to be updated in fwr
 * @param_value: vdev parameter value
 *
 * Return: QDF status
 */
QDF_STATUS pmo_tgt_vdev_update_param_req(struct wlan_objmgr_vdev *vdev,
		enum pmo_vdev_param_id param_id, uint32_t param_value);

/**
 * pmo_tgt_send_vdev_sta_ps_param() - Send vdev sta power save param to fwr
 * @vdev: objmgr vdev
 * @ps_param: sta mode ps power save params type
 * @param_value: power save parameter value
 *
 * Return: QDF status
 */
QDF_STATUS pmo_tgt_send_vdev_sta_ps_param(struct wlan_objmgr_vdev *vdev,
		enum pmo_sta_powersave_param ps_param, uint32_t param_value);

#ifdef WLAN_FEATURE_IGMP_OFFLOAD
/**
 * pmo_tgt_send_igmp_offload_req() - Send igmp offload request to fw
 * @vdev: objmgr vdev
 * @pmo_igmp_req: igmp offload params
 *
 * Return: QDF status
 */
QDF_STATUS
pmo_tgt_send_igmp_offload_req(struct wlan_objmgr_vdev *vdev,
			      struct pmo_igmp_offload_req *pmo_igmp_req);
#else
static inline QDF_STATUS
pmo_tgt_send_igmp_offload_req(struct wlan_objmgr_vdev *vdev,
			      struct pmo_igmp_offload_req *pmo_igmp_req)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * pmo_tgt_update_wow_bus_suspend_state() - update wow bus suspend state flag
 * @psoc: objmgr psoc
 * @val: true for setting wow suspend flag to set else false
 *
 * Return: None
 */
void pmo_tgt_psoc_update_wow_bus_suspend_state(struct wlan_objmgr_psoc *psoc,
		uint8_t val);

/**
 * pmo_tgt_get_host_credits() - Get host credits
 * @psoc: objmgr psoc
 *
 * Return: Pending WMI commands on success else EAGAIN on error
 */
int pmo_tgt_psoc_get_host_credits(struct wlan_objmgr_psoc *psoc);

/**
 * pmo_tgt_get_pending_cmnds() - Get pending commands
 * @psoc: objmgr psoc
 *
 * Return: Pending WMI commands on success else EAGAIN on error
 */
int pmo_tgt_psoc_get_pending_cmnds(struct wlan_objmgr_psoc *psoc);

/**
 * pmo_tgt_update_target_suspend_flag() - Set WMI target Suspend flag
 * @psoc: objmgr psoc
 * @val: true on suspend false for resume
 *
 * Return: None
 */
void pmo_tgt_update_target_suspend_flag(struct wlan_objmgr_psoc *psoc,
					uint8_t val);

/**
 * pmo_tgt_update_target_suspend_acked_flag() - Set WMI target Suspend acked
 *                                              flag
 * @psoc: objmgr psoc
 * @val: true on suspend false for resume
 *
 * Return: None
 */
void pmo_tgt_update_target_suspend_acked_flag(struct wlan_objmgr_psoc *psoc,
					      uint8_t val);

/**
 * pmo_tgt_is_target_suspended() - Get WMI target Suspend flag
 * @psoc: objmgr psoc
 *
 * Return: true if target suspended, false otherwise.
 */
bool pmo_tgt_is_target_suspended(struct wlan_objmgr_psoc *psoc);

/**
 * pmo_tgt_psoc_send_wow_enable_req() -Send wow enable request
 * @psoc: objmgr psoc
 * @param: WOW enable request buffer
 *
 * Return: QDF_STATUS_SUCCESS on success else error code
 */
QDF_STATUS pmo_tgt_psoc_send_wow_enable_req(struct wlan_objmgr_psoc *psoc,
	struct pmo_wow_cmd_params *param);

/**
 * pmo_tgt_psoc_send_supend_req() -Send target suspend request to fwr
 * @psoc: objmgr psoc
 * @param: suspend request buffer
 *
 * Return: QDF_STATUS_SUCCESS on success else error code
 */
QDF_STATUS pmo_tgt_psoc_send_supend_req(struct wlan_objmgr_psoc *psoc,
		struct pmo_suspend_params *param);

/**
 * pmo_tgt_psoc_set_runtime_pm_inprogress() -set runtime status
 * @psoc: objmgr psoc
 * @value: set runtime pm in progress true or false
 *
 * Return: none
 */
QDF_STATUS pmo_tgt_psoc_set_runtime_pm_inprogress(struct wlan_objmgr_psoc *psoc,
						  bool value);

/**
 * pmo_tgt_psoc_get_runtime_pm_in_progress() -get runtime status
 * @psoc: objmgr psoc
 *
 * Return: true if runtime pm is in progress else false
 */
bool pmo_tgt_psoc_get_runtime_pm_in_progress(struct wlan_objmgr_psoc *psoc);

/**
 * pmo_tgt_psoc_send_host_wakeup_ind() -Send host wake up indication to fwr
 * @psoc: objmgr psoc
 *
 * Return: QDF_STATUS_SUCCESS on success else error code
 */
QDF_STATUS pmo_tgt_psoc_send_host_wakeup_ind(struct wlan_objmgr_psoc *psoc);

/**
 * pmo_tgt_psoc_send_target_resume_req() -Send target resume request
 * @psoc: objmgr psoc
 *
 * Return: QDF_STATUS_SUCCESS on success else error code
 */
QDF_STATUS pmo_tgt_psoc_send_target_resume_req(struct wlan_objmgr_psoc *psoc);

/**
 * pmo_tgt_psoc_send_idle_roam_monitor() - Send idle roam set suspend mode
 * command to firmware
 * @psoc: objmgr psoc
 * @val: Set suspend mode value
 *
 * Return: QDF_STATUS_SUCCESS on success else error code
 */
QDF_STATUS pmo_tgt_psoc_send_idle_roam_monitor(struct wlan_objmgr_psoc *psoc,
					       uint8_t val);

#endif /* end  of _WLAN_PMO_TGT_API_H_ */
