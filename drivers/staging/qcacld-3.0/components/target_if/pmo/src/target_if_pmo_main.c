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
 * DOC: Target interface file for pmo component to
 * Implement api's which shall be used by pmo component
 * in target if internally.
 */

#include "target_if_pmo.h"
#include "wlan_pmo_common_public_struct.h"

#ifdef WLAN_FEATURE_PACKET_FILTERING
static inline
void tgt_if_pmo_reg_pkt_filter_ops(struct wlan_pmo_tx_ops *pmo_tx_ops)
{
	pmo_tx_ops->send_set_pkt_filter =
		target_if_pmo_send_pkt_filter_req;
	pmo_tx_ops->send_clear_pkt_filter =
		target_if_pmo_clear_pkt_filter_req;
}
#else
static inline void
tgt_if_pmo_reg_pkt_filter_ops(struct wlan_pmo_tx_ops *pmo_tx_ops)
{
}
#endif
#ifdef WLAN_FEATURE_IGMP_OFFLOAD
static void
update_pmo_igmp_tx_ops(struct wlan_pmo_tx_ops *pmo_tx_ops)
{
	pmo_tx_ops->send_igmp_offload_req =
		target_if_pmo_send_igmp_offload_req;
}
#else
static inline void
update_pmo_igmp_tx_ops(struct wlan_pmo_tx_ops *pmo_tx_ops)
{}
#endif
void target_if_pmo_register_tx_ops(struct wlan_pmo_tx_ops *pmo_tx_ops)
{
	if (!pmo_tx_ops) {
		target_if_err("pmo_tx_ops is null");
		return;
	}

	pmo_tx_ops->send_arp_offload_req =
		target_if_pmo_send_arp_offload_req;
	pmo_tx_ops->send_conf_hw_filter_req =
		target_if_pmo_conf_hw_filter;
	pmo_tx_ops->send_ns_offload_req =
		target_if_pmo_send_ns_offload_req;
	pmo_tx_ops->send_enable_wow_wakeup_event_req =
		target_if_pmo_enable_wow_wakeup_event;
	pmo_tx_ops->send_disable_wow_wakeup_event_req =
		target_if_pmo_disable_wow_wakeup_event;
	pmo_tx_ops->send_add_wow_pattern =
		target_if_pmo_send_wow_patterns_to_fw;
	pmo_tx_ops->del_wow_pattern =
		target_if_pmo_del_wow_patterns_to_fw;
	pmo_tx_ops->send_enhance_mc_offload_req =
		target_if_pmo_send_enhance_mc_offload_req;
	pmo_tx_ops->send_set_mc_filter_req =
		target_if_pmo_set_mc_filter_req;
	pmo_tx_ops->send_clear_mc_filter_req =
		target_if_pmo_clear_mc_filter_req;
	pmo_tx_ops->get_multiple_mc_filter_support =
		target_if_pmo_get_multiple_mc_filter_support;
	pmo_tx_ops->send_set_multiple_mc_filter_req =
		target_if_pmo_set_multiple_mc_filter_req;
	pmo_tx_ops->send_clear_multiple_mc_filter_req =
		target_if_pmo_clear_multiple_mc_filter_req;
	pmo_tx_ops->send_ra_filter_req =
		target_if_pmo_send_ra_filter_req;
	pmo_tx_ops->send_gtk_offload_req =
		target_if_pmo_send_gtk_offload_req;
	pmo_tx_ops->send_get_gtk_rsp_cmd =
		target_if_pmo_send_gtk_response_req;
	pmo_tx_ops->send_action_frame_pattern_req =
		target_if_pmo_send_action_frame_patterns;
	pmo_tx_ops->send_lphb_enable =
		target_if_pmo_send_lphb_enable;
	pmo_tx_ops->send_lphb_tcp_params =
		target_if_pmo_send_lphb_tcp_params;
	pmo_tx_ops->send_lphb_tcp_filter_req =
		target_if_pmo_send_lphb_tcp_pkt_filter;
	pmo_tx_ops->send_lphb_upd_params =
		target_if_pmo_send_lphb_udp_params;
	pmo_tx_ops->send_lphb_udp_filter_req =
		target_if_pmo_send_lphb_udp_pkt_filter;
	pmo_tx_ops->send_vdev_param_update_req =
		target_if_pmo_send_vdev_update_param_req;
	pmo_tx_ops->send_vdev_sta_ps_param_req =
		target_if_pmo_send_vdev_ps_param_req;
	pmo_tx_ops->psoc_update_wow_bus_suspend =
		target_if_pmo_psoc_update_bus_suspend;
	pmo_tx_ops->psoc_get_host_credits =
		target_if_pmo_psoc_get_host_credits;
	update_pmo_igmp_tx_ops(pmo_tx_ops);
	pmo_tx_ops->psoc_get_pending_cmnds =
		target_if_pmo_psoc_get_pending_cmnds;
	pmo_tx_ops->update_target_suspend_flag =
		target_if_pmo_update_target_suspend_flag;
	pmo_tx_ops->update_target_suspend_acked_flag =
		target_if_pmo_update_target_suspend_acked_flag;
	pmo_tx_ops->is_target_suspended =
		target_if_pmo_is_target_suspended;
	pmo_tx_ops->psoc_send_wow_enable_req =
		target_if_pmo_psoc_send_wow_enable_req;
	pmo_tx_ops->psoc_send_supend_req =
		target_if_pmo_psoc_send_suspend_req;
	pmo_tx_ops->psoc_set_runtime_pm_in_progress =
		target_if_pmo_set_runtime_pm_in_progress;
	pmo_tx_ops->psoc_get_runtime_pm_in_progress =
		target_if_pmo_get_runtime_pm_in_progress;
	pmo_tx_ops->psoc_send_host_wakeup_ind =
		target_if_pmo_psoc_send_host_wakeup_ind;
	pmo_tx_ops->psoc_send_target_resume_req =
		target_if_pmo_psoc_send_target_resume_req;
	pmo_tx_ops->psoc_send_d0wow_enable_req =
		target_if_pmo_psoc_send_d0wow_enable_req;
	pmo_tx_ops->psoc_send_d0wow_disable_req =
		target_if_pmo_psoc_send_d0wow_disable_req;
	pmo_tx_ops->psoc_send_idle_roam_suspend_mode =
		target_if_pmo_psoc_send_idle_monitor_cmd;
	tgt_if_pmo_reg_pkt_filter_ops(pmo_tx_ops);
}

