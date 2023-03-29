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
 * DOC: public API related to the pmo called by north bound HDD/OSIF
 */

#include "wlan_pmo_ucfg_api.h"
#include "wlan_pmo_apf.h"
#include "wlan_pmo_arp.h"
#include "wlan_pmo_ns.h"
#include "wlan_pmo_gtk.h"
#include "wlan_pmo_wow.h"
#include "wlan_pmo_mc_addr_filtering.h"
#include "wlan_pmo_main.h"
#include "wlan_pmo_lphb.h"
#include "wlan_pmo_suspend_resume.h"
#include "wlan_pmo_pkt_filter.h"
#include "wlan_pmo_hw_filter.h"
#include "wlan_pmo_cfg.h"
#include "wlan_pmo_static_config.h"
#include "cfg_ucfg_api.h"

QDF_STATUS ucfg_pmo_psoc_open(struct wlan_objmgr_psoc *psoc)
{
	return pmo_psoc_open(psoc);
}

QDF_STATUS ucfg_pmo_psoc_close(struct wlan_objmgr_psoc *psoc)
{
	return pmo_psoc_close(psoc);
}

uint32_t ucfg_pmo_get_apf_instruction_size(struct wlan_objmgr_psoc *psoc)
{
	QDF_BUG(psoc);
	if (!psoc)
		return 0;

	return pmo_get_apf_instruction_size(psoc);
}

uint8_t ucfg_pmo_get_num_wow_filters(struct wlan_objmgr_psoc *psoc)
{
	QDF_BUG(psoc);
	if (!psoc)
		return 0;

	return pmo_get_num_wow_filters(psoc);
}

QDF_STATUS ucfg_pmo_get_psoc_config(struct wlan_objmgr_psoc *psoc,
		struct pmo_psoc_cfg *psoc_cfg)
{
	return pmo_core_get_psoc_config(psoc, psoc_cfg);
}

QDF_STATUS ucfg_pmo_update_psoc_config(struct wlan_objmgr_psoc *psoc,
		struct pmo_psoc_cfg *psoc_cfg)
{
	return pmo_core_update_psoc_config(psoc, psoc_cfg);
}

QDF_STATUS ucfg_pmo_psoc_set_caps(struct wlan_objmgr_psoc *psoc,
				  struct pmo_device_caps *caps)
{
	QDF_BUG(psoc);
	if (!psoc)
		return QDF_STATUS_E_INVAL;

	QDF_BUG(caps);
	if (!caps)
		return QDF_STATUS_E_INVAL;

	pmo_psoc_set_caps(psoc, caps);

	return QDF_STATUS_SUCCESS;
}

bool ucfg_pmo_is_ap_mode_supports_arp_ns(struct wlan_objmgr_psoc *psoc,
	enum QDF_OPMODE vdev_opmode)
{
	return pmo_core_is_ap_mode_supports_arp_ns(psoc, vdev_opmode);
}

bool ucfg_pmo_is_vdev_connected(struct wlan_objmgr_vdev *vdev)
{
	if (wlan_vdev_is_up(vdev) == QDF_STATUS_SUCCESS)
		return true;
	else
		return false;
}

bool ucfg_pmo_is_vdev_supports_offload(struct wlan_objmgr_vdev *vdev)
{
	return pmo_core_is_vdev_supports_offload(vdev);
}

void ucfg_pmo_enable_wakeup_event(struct wlan_objmgr_psoc *psoc,
				  uint32_t vdev_id,
				  WOW_WAKE_EVENT_TYPE wow_event)
{
	pmo_core_enable_wakeup_event(psoc, vdev_id, wow_event);
}

void ucfg_pmo_disable_wakeup_event(struct wlan_objmgr_psoc *psoc,
				   uint32_t vdev_id,
				   WOW_WAKE_EVENT_TYPE wow_event)
{
	pmo_core_disable_wakeup_event(psoc, vdev_id, wow_event);
}

QDF_STATUS ucfg_pmo_cache_arp_offload_req(struct pmo_arp_req *arp_req)
{
	return pmo_core_cache_arp_offload_req(arp_req);
}

QDF_STATUS ucfg_pmo_check_arp_offload(struct wlan_objmgr_psoc *psoc,
				      enum pmo_offload_trigger trigger,
				      uint8_t vdev_id)
{
	return pmo_core_arp_check_offload(psoc, trigger, vdev_id);
}

QDF_STATUS ucfg_pmo_flush_arp_offload_req(struct wlan_objmgr_vdev *vdev)
{
	return pmo_core_flush_arp_offload_req(vdev);
}

QDF_STATUS ucfg_pmo_enable_arp_offload_in_fwr(struct wlan_objmgr_vdev *vdev,
					      enum pmo_offload_trigger trigger)
{
	return pmo_core_enable_arp_offload_in_fwr(vdev, trigger);
}

QDF_STATUS
ucfg_pmo_disable_arp_offload_in_fwr(struct wlan_objmgr_vdev *vdev,
				    enum pmo_offload_trigger trigger)
{
	return pmo_core_disable_arp_offload_in_fwr(vdev, trigger);
}

QDF_STATUS
ucfg_pmo_get_arp_offload_params(struct wlan_objmgr_vdev *vdev,
				struct pmo_arp_offload_params *params)
{
	return pmo_core_get_arp_offload_params(vdev, params);
}

#ifdef WLAN_NS_OFFLOAD
QDF_STATUS ucfg_pmo_cache_ns_offload_req(struct pmo_ns_req *ns_req)
{
	return pmo_core_cache_ns_offload_req(ns_req);
}

QDF_STATUS ucfg_pmo_ns_offload_check(struct wlan_objmgr_psoc *psoc,
				     enum pmo_offload_trigger trigger,
				     uint8_t vdev_id)
{
	return pmo_core_ns_check_offload(psoc, trigger, vdev_id);
}

QDF_STATUS ucfg_pmo_flush_ns_offload_req(struct wlan_objmgr_vdev *vdev)
{
	return pmo_core_flush_ns_offload_req(vdev);
}

QDF_STATUS
ucfg_pmo_enable_ns_offload_in_fwr(struct wlan_objmgr_vdev *vdev,
				  enum pmo_offload_trigger trigger)
{
	return pmo_core_enable_ns_offload_in_fwr(vdev, trigger);
}

QDF_STATUS
ucfg_pmo_disable_ns_offload_in_fwr(struct wlan_objmgr_vdev *vdev,
				   enum pmo_offload_trigger trigger)
{
	return pmo_core_disable_ns_offload_in_fwr(vdev, trigger);
}
#endif /* WLAN_NS_OFFLOAD */

QDF_STATUS
ucfg_pmo_get_ns_offload_params(struct wlan_objmgr_vdev *vdev,
			       struct pmo_ns_offload_params *params)
{
	return pmo_core_get_ns_offload_params(vdev, params);
}

enum pmo_ns_addr_scope
ucfg_pmo_ns_addr_scope(uint32_t ipv6_scope)
{
	switch (ipv6_scope) {
	case IPV6_ADDR_SCOPE_NODELOCAL:
		return PMO_NS_ADDR_SCOPE_NODELOCAL;
	case IPV6_ADDR_SCOPE_LINKLOCAL:
		return PMO_NS_ADDR_SCOPE_LINKLOCAL;
	case IPV6_ADDR_SCOPE_SITELOCAL:
		return PMO_NS_ADDR_SCOPE_SITELOCAL;
	case IPV6_ADDR_SCOPE_ORGLOCAL:
		return PMO_NS_ADDR_SCOPE_ORGLOCAL;
	case IPV6_ADDR_SCOPE_GLOBAL:
		return PMO_NS_ADDR_SCOPE_GLOBAL;
	}

	return PMO_NS_ADDR_SCOPE_INVALID;
}

QDF_STATUS ucfg_pmo_cache_mc_addr_list(
		struct pmo_mc_addr_list_params *mc_list_config)
{
	return pmo_core_cache_mc_addr_list(mc_list_config);
}

QDF_STATUS ucfg_pmo_flush_mc_addr_list(struct wlan_objmgr_psoc *psoc,
				       uint8_t vdev_id)
{
	return pmo_core_flush_mc_addr_list(psoc, vdev_id);
}

QDF_STATUS ucfg_pmo_enable_mc_addr_filtering_in_fwr(
		struct wlan_objmgr_psoc *psoc,
		uint8_t vdev_id,
		enum pmo_offload_trigger trigger)
{
	return pmo_core_enable_mc_addr_filtering_in_fwr(psoc,
			vdev_id, trigger);
}

QDF_STATUS ucfg_pmo_disable_mc_addr_filtering_in_fwr(
		struct wlan_objmgr_psoc *psoc,
		uint8_t vdev_id,
		enum pmo_offload_trigger trigger)
{
	return pmo_core_disable_mc_addr_filtering_in_fwr(psoc,
			vdev_id, trigger);
}

uint8_t ucfg_pmo_max_mc_addr_supported(struct wlan_objmgr_psoc *psoc)
{
	return pmo_core_max_mc_addr_supported(psoc);
}

QDF_STATUS
ucfg_pmo_get_mc_addr_list(struct wlan_objmgr_psoc *psoc,
			  uint8_t vdev_id,
			  struct pmo_mc_addr_list *mc_list_req)
{
	return pmo_core_get_mc_addr_list(psoc, vdev_id, mc_list_req);
}

QDF_STATUS
ucfg_pmo_cache_gtk_offload_req(struct wlan_objmgr_vdev *vdev,
			       struct pmo_gtk_req *gtk_req)
{
	return pmo_core_cache_gtk_offload_req(vdev, gtk_req);
}

QDF_STATUS ucfg_pmo_flush_gtk_offload_req(struct wlan_objmgr_vdev *vdev)
{
	return pmo_core_flush_gtk_offload_req(vdev);
}

QDF_STATUS ucfg_pmo_enable_gtk_offload_in_fwr(struct wlan_objmgr_vdev *vdev)
{
	return pmo_core_enable_gtk_offload_in_fwr(vdev);
}

#ifdef WLAN_FEATURE_IGMP_OFFLOAD
QDF_STATUS ucfg_pmo_enable_igmp_offload(struct wlan_objmgr_vdev *vdev,
					struct pmo_igmp_offload_req *igmp_req)
{
	return pmo_core_enable_igmp_offload(vdev, igmp_req);
}
#endif

QDF_STATUS ucfg_pmo_disable_gtk_offload_in_fwr(struct wlan_objmgr_vdev *vdev)
{
	return pmo_core_disable_gtk_offload_in_fwr(vdev);
}

#ifdef WLAN_FEATURE_PACKET_FILTERING
uint8_t ucfg_pmo_get_pkt_filter_bitmap(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.packet_filters_bitmap;
}

uint32_t ucfg_pmo_get_num_packet_filters(struct wlan_objmgr_psoc *psoc)
{
	QDF_BUG(psoc);
	if (!psoc)
		return 0;

	return pmo_get_num_packet_filters(psoc);
}

QDF_STATUS
ucfg_pmo_set_pkt_filter(struct wlan_objmgr_psoc *psoc,
			struct pmo_rcv_pkt_fltr_cfg *pmo_set_pkt_fltr_req,
			uint8_t vdev_id)
{
	return pmo_core_set_pkt_filter(psoc, pmo_set_pkt_fltr_req, vdev_id);
}

QDF_STATUS ucfg_pmo_clear_pkt_filter(
		struct wlan_objmgr_psoc *psoc,
		struct pmo_rcv_pkt_fltr_clear_param *pmo_clr_pkt_fltr_param,
		uint8_t vdev_id)
{
	return pmo_core_clear_pkt_filter(psoc,
				pmo_clr_pkt_fltr_param, vdev_id);
}
#endif

QDF_STATUS ucfg_pmo_get_gtk_rsp(struct wlan_objmgr_vdev *vdev,
				struct pmo_gtk_rsp_req *gtk_rsp_req)
{
	return pmo_core_get_gtk_rsp(vdev, gtk_rsp_req);
}

void ucfg_pmo_update_extscan_in_progress(struct wlan_objmgr_vdev *vdev,
					 bool value)
{
	pmo_core_update_extscan_in_progress(vdev, value);
}

void ucfg_pmo_update_p2plo_in_progress(struct wlan_objmgr_vdev *vdev,
				       bool value)
{
	pmo_core_update_p2plo_in_progress(vdev, value);
}

QDF_STATUS ucfg_pmo_lphb_config_req(struct wlan_objmgr_psoc *psoc,
				    struct pmo_lphb_req *lphb_req,
				    void *lphb_cb_ctx,
				    pmo_lphb_callback callback)
{
	return pmo_core_lphb_config_req(psoc, lphb_req, lphb_cb_ctx, callback);
}

void ucfg_pmo_psoc_update_power_save_mode(struct wlan_objmgr_psoc *psoc,
					  uint8_t value)
{
	pmo_core_psoc_update_power_save_mode(psoc, value);
}

void ucfg_pmo_psoc_update_dp_handle(struct wlan_objmgr_psoc *psoc,
				    void *dp_handle)
{
	pmo_core_psoc_update_dp_handle(psoc, dp_handle);
}

void ucfg_pmo_psoc_update_htc_handle(struct wlan_objmgr_psoc *psoc,
				     void *htc_handle)
{
	pmo_core_psoc_update_htc_handle(psoc, htc_handle);
}

void ucfg_pmo_psoc_set_hif_handle(struct wlan_objmgr_psoc *psoc,
				  void *hif_handle)
{
	pmo_core_psoc_set_hif_handle(psoc, hif_handle);
}

void ucfg_pmo_psoc_set_txrx_pdev_id(struct wlan_objmgr_psoc *psoc,
				    uint8_t txrx_pdev_id)
{
	pmo_core_psoc_set_txrx_pdev_id(psoc, txrx_pdev_id);
}

void ucfg_pmo_psoc_handle_initial_wake_up(void *cb_ctx)
{
	return pmo_core_psoc_handle_initial_wake_up(cb_ctx);
}

QDF_STATUS
ucfg_pmo_psoc_user_space_suspend_req(struct wlan_objmgr_psoc *psoc,
				     enum qdf_suspend_type type)
{
	return pmo_core_psoc_user_space_suspend_req(psoc, type);
}

QDF_STATUS
ucfg_pmo_psoc_user_space_resume_req(struct wlan_objmgr_psoc *psoc,
				    enum qdf_suspend_type type)
{
	return pmo_core_psoc_user_space_resume_req(psoc, type);
}

QDF_STATUS ucfg_pmo_suspend_all_components(struct wlan_objmgr_psoc *psoc,
					   enum qdf_suspend_type type)
{
	return pmo_suspend_all_components(psoc, type);
}

QDF_STATUS ucfg_pmo_resume_all_components(struct wlan_objmgr_psoc *psoc,
					  enum qdf_suspend_type type)
{
	return pmo_resume_all_components(psoc, type);
}

QDF_STATUS
ucfg_pmo_psoc_bus_suspend_req(struct wlan_objmgr_psoc *psoc,
			      enum qdf_suspend_type type,
			      struct pmo_wow_enable_params *wow_params)
{
	return pmo_core_psoc_bus_suspend_req(psoc, type, wow_params);
}

#ifdef FEATURE_RUNTIME_PM
QDF_STATUS ucfg_pmo_psoc_bus_runtime_suspend(struct wlan_objmgr_psoc *psoc,
					     pmo_pld_auto_suspend_cb pld_cb)
{
	return pmo_core_psoc_bus_runtime_suspend(psoc, pld_cb);
}

QDF_STATUS ucfg_pmo_psoc_bus_runtime_resume(struct wlan_objmgr_psoc *psoc,
					    pmo_pld_auto_suspend_cb pld_cb)
{
	return pmo_core_psoc_bus_runtime_resume(psoc, pld_cb);
}
#endif

QDF_STATUS
ucfg_pmo_psoc_suspend_target(struct wlan_objmgr_psoc *psoc,
			     int disable_target_intr)
{
	return pmo_core_psoc_suspend_target(psoc, disable_target_intr);
}

QDF_STATUS
ucfg_pmo_add_wow_user_pattern(struct wlan_objmgr_vdev *vdev,
			      struct pmo_wow_add_pattern *ptrn)
{
	return pmo_core_add_wow_user_pattern(vdev, ptrn);
}

QDF_STATUS
ucfg_pmo_del_wow_pattern(struct wlan_objmgr_vdev *vdev)
{
	return  pmo_core_del_wow_pattern(vdev);
}

QDF_STATUS
ucfg_pmo_del_wow_user_pattern(struct wlan_objmgr_vdev *vdev,
			      uint8_t pattern_id)
{
	return pmo_core_del_wow_user_pattern(vdev, pattern_id);
}

QDF_STATUS
ucfg_pmo_psoc_bus_resume_req(struct wlan_objmgr_psoc *psoc,
			     enum qdf_suspend_type type)
{
	return pmo_core_psoc_bus_resume_req(psoc, type);
}

bool ucfg_pmo_get_wow_bus_suspend(struct wlan_objmgr_psoc *psoc)
{
	return pmo_core_get_wow_bus_suspend(psoc);
}

int ucfg_pmo_psoc_is_target_wake_up_received(struct wlan_objmgr_psoc *psoc)
{
	return pmo_core_psoc_is_target_wake_up_received(psoc);
}

int ucfg_pmo_psoc_clear_target_wake_up(struct wlan_objmgr_psoc *psoc)
{
	return pmo_core_psoc_clear_target_wake_up(psoc);
}

void ucfg_pmo_psoc_target_suspend_acknowledge(void *context, bool wow_nack)
{
	pmo_core_psoc_target_suspend_acknowledge(context, wow_nack);
}

void ucfg_pmo_psoc_wakeup_host_event_received(struct wlan_objmgr_psoc *psoc)
{
	pmo_core_psoc_wakeup_host_event_received(psoc);
}

QDF_STATUS ucfg_pmo_enable_hw_filter_in_fwr(struct wlan_objmgr_vdev *vdev)
{
	return pmo_core_enable_hw_filter_in_fwr(vdev);
}

QDF_STATUS
ucfg_pmo_enable_action_frame_patterns(struct wlan_objmgr_vdev *vdev,
				      enum qdf_suspend_type suspend_type)
{
	return pmo_register_action_frame_patterns(vdev, suspend_type);
}

QDF_STATUS ucfg_pmo_disable_action_frame_patterns(struct wlan_objmgr_vdev *vdev)
{
	return pmo_clear_action_frame_patterns(vdev);
}

QDF_STATUS ucfg_pmo_disable_hw_filter_in_fwr(struct wlan_objmgr_vdev *vdev)
{
	return pmo_core_disable_hw_filter_in_fwr(vdev);
}

QDF_STATUS ucfg_pmo_config_listen_interval(struct wlan_objmgr_vdev *vdev,
					   uint32_t listen_interval)
{
	return pmo_core_config_listen_interval(vdev, listen_interval);
}

QDF_STATUS ucfg_pmo_config_modulated_dtim(struct wlan_objmgr_vdev *vdev,
					  uint32_t mod_dtim)
{
	return pmo_core_config_modulated_dtim(vdev, mod_dtim);
}

enum pmo_wow_enable_type
ucfg_pmo_get_wow_enable(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.wow_enable;
}

void
ucfg_pmo_set_wow_enable(struct wlan_objmgr_psoc *psoc,
			enum pmo_wow_enable_type val)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	pmo_psoc_ctx->psoc_cfg.wow_enable = val;
}

bool
ucfg_pmo_is_arp_offload_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.arp_offload_enable;
}

#ifdef WLAN_FEATURE_IGMP_OFFLOAD
bool
ucfg_pmo_is_igmp_offload_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.igmp_offload_enable;
}
#endif

void
ucfg_pmo_set_arp_offload_enabled(struct wlan_objmgr_psoc *psoc,
				 bool val)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	pmo_psoc_ctx->psoc_cfg.arp_offload_enable = val;
}

#ifdef WLAN_FEATURE_IGMP_OFFLOAD
void
ucfg_pmo_set_igmp_offload_enabled(struct wlan_objmgr_psoc *psoc,
				  bool val)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	pmo_psoc_ctx->psoc_cfg.igmp_offload_enable = val;
}
#endif

enum pmo_auto_pwr_detect_failure_mode
ucfg_pmo_get_auto_power_fail_mode(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.auto_power_save_fail_mode;
}

#ifdef WLAN_FEATURE_WOW_PULSE
bool ucfg_pmo_is_wow_pulse_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.is_wow_pulse_supported;
}

uint8_t ucfg_pmo_get_wow_pulse_pin(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.wow_pulse_pin;
}

uint16_t ucfg_pmo_get_wow_pulse_interval_high(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.wow_pulse_interval_high;
}

uint16_t ucfg_pmo_get_wow_pulse_interval_low(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.wow_pulse_interval_low;
}

uint32_t ucfg_pmo_get_wow_pulse_repeat_count(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.wow_pulse_repeat_count;
}

uint32_t ucfg_pmo_get_wow_pulse_init_state(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.wow_pulse_init_state;
}
#endif

bool ucfg_pmo_is_active_mode_offloaded(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.active_mode_offload;
}

#ifdef FEATURE_WLAN_APF
bool ucfg_pmo_is_apf_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_intersect_apf(pmo_psoc_ctx);
}
#endif

bool
ucfg_pmo_is_ssdp_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.ssdp;
}

#ifdef FEATURE_RUNTIME_PM
uint32_t
ucfg_pmo_get_runtime_pm_delay(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.runtime_pm_delay;
}
#endif /* FEATURE_RUNTIME_PM */

bool
ucfg_pmo_is_ns_offloaded(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.ns_offload_enable_static;
}

uint8_t
ucfg_pmo_get_sta_dynamic_dtim(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.sta_dynamic_dtim;
}

uint8_t
ucfg_pmo_get_sta_mod_dtim(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.sta_mod_dtim;
}

void
ucfg_pmo_set_sta_mod_dtim(struct wlan_objmgr_psoc *psoc,
			  uint8_t val)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	pmo_psoc_ctx->psoc_cfg.sta_mod_dtim = val;
}

bool
ucfg_pmo_is_mc_addr_list_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.enable_mc_list;
}

enum powersave_mode
ucfg_pmo_get_power_save_mode(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.power_save_mode;
}

enum powersave_mode
ucfg_pmo_get_default_power_save_mode(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.default_power_save_mode;
}

void
ucfg_pmo_set_power_save_mode(struct wlan_objmgr_psoc *psoc,
			     enum powersave_mode val)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	pmo_psoc_ctx->psoc_cfg.power_save_mode = val;
}

uint8_t
ucfg_pmo_get_max_ps_poll(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.max_ps_poll;
}

uint8_t
ucfg_pmo_power_save_offload_enabled(struct wlan_objmgr_psoc *psoc)
{
	uint8_t powersave_offload_enabled = PMO_PS_ADVANCED_POWER_SAVE_ENABLE;
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	if (!pmo_psoc_ctx->psoc_cfg.max_ps_poll ||
	    !pmo_psoc_ctx->psoc_cfg.power_save_mode)
		powersave_offload_enabled =
			pmo_psoc_ctx->psoc_cfg.power_save_mode;

	pmo_debug("powersave offload enabled type:%d",
		  powersave_offload_enabled);

	return powersave_offload_enabled;
}

QDF_STATUS
ucfg_pmo_tgt_psoc_send_idle_roam_suspend_mode(struct wlan_objmgr_psoc *psoc,
					      uint8_t val)
{
	return pmo_tgt_psoc_send_idle_roam_monitor(psoc, val);
}

#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
bool
ucfg_pmo_extwow_is_goto_suspend_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.extwow_goto_suspend;
}

uint8_t
ucfg_pmo_extwow_app1_wakeup_pin_num(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.extwow_app1_wakeup_pin_num;
}

uint8_t
ucfg_pmo_extwow_app2_wakeup_pin_num(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.extwow_app2_wakeup_pin_num;
}

uint32_t
ucfg_pmo_extwow_app2_init_ping_interval(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.extwow_app2_init_ping_interval;
}

uint32_t
ucfg_pmo_extwow_app2_min_ping_interval(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.extwow_app2_min_ping_interval;
}

uint32_t
ucfg_pmo_extwow_app2_max_ping_interval(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.extwow_app2_max_ping_interval;
}

uint32_t
ucfg_pmo_extwow_app2_inc_ping_interval(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.extwow_app2_inc_ping_interval;
}

uint16_t
ucfg_pmo_extwow_app2_tcp_src_port(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.extwow_app2_tcp_src_port;
}

uint16_t
ucfg_pmo_extwow_app2_tcp_dst_port(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.extwow_app2_tcp_dst_port;
}

uint32_t
ucfg_pmo_extwow_app2_tcp_tx_timeout(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.extwow_app2_tcp_tx_timeout;
}

uint32_t
ucfg_pmo_extwow_app2_tcp_rx_timeout(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.extwow_app2_tcp_rx_timeout;
}
#endif

bool
ucfg_pmo_get_enable_sap_suspend(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.enable_sap_suspend;
}

void
ucfg_pmo_set_wow_data_inactivity_timeout(struct wlan_objmgr_psoc *psoc,
					 uint8_t val)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	pmo_psoc_ctx->psoc_cfg.wow_data_inactivity_timeout = val;
}

bool ucfg_pmo_is_pkt_filter_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.packet_filter_enabled;
}

enum active_apf_mode
ucfg_pmo_get_active_uc_apf_mode(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.active_uc_apf_mode;
}

enum active_apf_mode
ucfg_pmo_get_active_mc_bc_apf_mode(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.active_mc_bc_apf_mode;
}

bool
ucfg_pmo_get_sap_mode_bus_suspend(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.is_bus_suspend_enabled_in_sap_mode;
}

bool
ucfg_pmo_get_go_mode_bus_suspend(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx = pmo_psoc_get_priv(psoc);

	return pmo_psoc_ctx->psoc_cfg.is_bus_suspend_enabled_in_go_mode;
}

QDF_STATUS ucfg_pmo_core_txrx_suspend(struct wlan_objmgr_psoc *psoc)
{
	return pmo_core_txrx_suspend(psoc);
}

QDF_STATUS ucfg_pmo_core_txrx_resume(struct wlan_objmgr_psoc *psoc)
{
	return pmo_core_txrx_resume(psoc);
}

#ifdef SYSTEM_PM_CHECK
void ucfg_pmo_notify_system_resume(struct wlan_objmgr_psoc *psoc)
{
	pmo_core_system_resume(psoc);
}
#endif
