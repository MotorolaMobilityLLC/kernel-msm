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
 * DOC: Declare public API related to the pmo called by north bound HDD/OSIF
 */

#ifndef _WLAN_PMO_UCFG_API_H_
#define _WLAN_PMO_UCFG_API_H_

#include "wlan_pmo_arp_public_struct.h"
#include "wlan_pmo_ns_public_struct.h"
#include "wlan_pmo_gtk_public_struct.h"
#include "wlan_pmo_mc_addr_filtering.h"
#include "wlan_pmo_mc_addr_filtering_public_struct.h"
#include "wlan_pmo_wow_public_struct.h"
#include "wlan_pmo_common_public_struct.h"
#include "wlan_pmo_obj_mgmt_api.h"
#include "wlan_pmo_pkt_filter_public_struct.h"
#include "wlan_pmo_hw_filter_public_struct.h"

#ifdef WLAN_POWER_MANAGEMENT_OFFLOAD

/**
 * ucfg_pmo_psoc_open() - pmo psoc object open
 * @psoc: objmgr vdev
 *.
 * This function used to open pmo psoc object by user space
 *
 * Return: true in case success else false
 */
QDF_STATUS ucfg_pmo_psoc_open(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_psoc_open() - pmo psoc object close
 * @psoc: objmgr vdev
 *.
 * This function used to close pmo psoc object by user space
 *
 * Return: true in case success else false
 */
QDF_STATUS ucfg_pmo_psoc_close(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_get_apf_instruction_size() - get the current APF instruction size
 * @psoc: the psoc to query
 *
 * Return: APF instruction size
 */
uint32_t ucfg_pmo_get_apf_instruction_size(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_get_num_wow_filters() - get the supported number of WoW filters
 * @psoc: the psoc to query
 *
 * Return: number of WoW filters supported
 */
uint8_t ucfg_pmo_get_num_wow_filters(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_is_ap_mode_supports_arp_ns() - Check ap mode support arp&ns offload
 * @psoc: objmgr psoc
 * @vdev_opmode: vdev opmode
 *
 * Return: true in case support else false
 */
bool ucfg_pmo_is_ap_mode_supports_arp_ns(struct wlan_objmgr_psoc *psoc,
	enum QDF_OPMODE vdev_opmode);

/**
 * ucfg_pmo_is_vdev_connected() -  to check whether peer is associated or not
 * @vdev: objmgr vdev
 *
 * Return: true in case success else false
 */
bool ucfg_pmo_is_vdev_connected(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_pmo_is_vdev_supports_offload() - check offload is supported on vdev
 * @vdev: objmgr vdev
 *
 * Return: true in case success else false
 */
bool ucfg_pmo_is_vdev_supports_offload(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_pmo_get_psoc_config(): API to get the psoc user configurations of pmo
 * @psoc: objmgr psoc handle
 * @psoc_cfg: fill the current psoc user configurations.
 *
 * Return pmo psoc configurations
 */
QDF_STATUS ucfg_pmo_get_psoc_config(struct wlan_objmgr_psoc *psoc,
		struct pmo_psoc_cfg *psoc_cfg);

/**
 * ucfg_pmo_update_psoc_config(): API to update the psoc user configurations
 * @psoc: objmgr psoc handle
 * @psoc_cfg: pmo psoc configurations
 *
 * This api shall be used for soc config initialization as well update.
 * In case of update caller must first call pmo_get_psoc_cfg to get
 * current config and then apply changes on top of current config.
 *
 * Return QDF_STATUS -in case of success else return error
 */
QDF_STATUS ucfg_pmo_update_psoc_config(struct wlan_objmgr_psoc *psoc,
		struct pmo_psoc_cfg *psoc_cfg);

/**
 * ucfg_pmo_psoc_set_caps() - overwrite configured device capability flags
 * @psoc: the psoc for which the capabilities apply
 * @caps: the cabability information to configure
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_pmo_psoc_set_caps(struct wlan_objmgr_psoc *psoc,
				  struct pmo_device_caps *caps);

/**
 * ucfg_pmo_is_arp_offload_enabled() - Get arp offload enable or not
 * @psoc: pointer to psoc object
 *
 * Return: arp offload enable or not
 */
bool
ucfg_pmo_is_arp_offload_enabled(struct wlan_objmgr_psoc *psoc);

#ifdef WLAN_FEATURE_IGMP_OFFLOAD
/**
 * ucfg_pmo_is_igmp_offload_enabled() - Get igmp offload enable or not
 * @psoc: pointer to psoc object
 *
 * Return: igmp offload enable or not
 */
bool
ucfg_pmo_is_igmp_offload_enabled(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_set_igmp_offload_enabled() - Set igmp offload enable or not
 * @psoc: pointer to psoc object
 * @val:  enable/disable igmp offload
 *
 * Return: None
 */
void
ucfg_pmo_set_igmp_offload_enabled(struct wlan_objmgr_psoc *psoc,
				  bool val);
#endif

/**
 * ucfg_pmo_set_arp_offload_enabled() - Set arp offload enable or not
 * @psoc: pointer to psoc object
 * @val:  enable/disable arp offload
 *
 * Return: None
 */
void
ucfg_pmo_set_arp_offload_enabled(struct wlan_objmgr_psoc *psoc,
				 bool val);

/**
 * ucfg_pmo_is_ssdp_enabled() - Get ssdp enable or not
 * @psoc: pointer to psoc object
 *
 * Return: enable/disable ssdp
 */
bool
ucfg_pmo_is_ssdp_enabled(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_is_ns_offloaded() - Get ns offload support or not
 * @psoc: pointer to psoc object
 *
 * Return: ns offload or not
 */
bool
ucfg_pmo_is_ns_offloaded(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_get_sta_dynamic_dtim() - Get dynamic dtim
 * @psoc: pointer to psoc object
 *
 * Return: dynamic dtim
 */
uint8_t
ucfg_pmo_get_sta_dynamic_dtim(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_get_sta_mod_dtim() - Get modulated dtim
 * @psoc: pointer to psoc object
 *
 * Return: modulated dtim
 */
uint8_t
ucfg_pmo_get_sta_mod_dtim(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_set_sta_mod_dtim() - Set modulated dtim
 * @psoc: pointer to psoc object
 * @val:  modulated dtim
 *
 * Return: None
 */
void
ucfg_pmo_set_sta_mod_dtim(struct wlan_objmgr_psoc *psoc,
			  uint8_t val);

/**
 * ucfg_pmo_is_mc_addr_list_enabled() - Get multicast address list enable or not
 * @psoc: pointer to psoc object
 *
 * Return: multicast address list enable or not
 */
bool
ucfg_pmo_is_mc_addr_list_enabled(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_get_power_save_mode() - Get power save mode
 * @psoc: pointer to psoc object
 *
 * Return: power save mode
 */
enum powersave_mode
ucfg_pmo_get_power_save_mode(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_get_default_power_save_mode() - Get default power save mode
 * from ini config
 * @psoc: pointer to psoc object
 *
 * Return: power save mode
 */
enum powersave_mode
ucfg_pmo_get_default_power_save_mode(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_set_power_save_mode() - Set power save mode
 * @psoc: pointer to psoc object
 * @val:  power save mode
 *
 * Return: None
 */
void
ucfg_pmo_set_power_save_mode(struct wlan_objmgr_psoc *psoc,
			     enum powersave_mode val);

/**
 * ucfg_pmo_get_max_ps_poll() - Get max power save poll
 * @psoc: pointer to psoc object
 *
 * Return: power save poll
 */
uint8_t
ucfg_pmo_get_max_ps_poll(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_power_save_offload_enabled() - Get power save offload enabled type
 * @psoc: pointer to psoc object
 *
 * Return: power save offload enabled type
 */
uint8_t
ucfg_pmo_power_save_offload_enabled(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_tgt_psoc_send_idle_roam_suspend_mode() - Send suspend mode to
 * firmware
 * @psoc: pointer to psoc object
 * @val: Set suspend mode on/off sent from userspace
 *
 * Return: QDF_STATUS_SUCCESS if suspend mode is sent to fw else return
 * corresponding QDF_STATUS failure code.
 */
QDF_STATUS
ucfg_pmo_tgt_psoc_send_idle_roam_suspend_mode(struct wlan_objmgr_psoc *psoc,
					      uint8_t val);

/**
 * ucfg_pmo_enable_wakeup_event() -  enable wow wakeup events
 * @psoc: objmgr psoc
 * @vdev_id: vdev id
 * @wow_event: wow event to enable
 *
 * Return: none
 */
void ucfg_pmo_enable_wakeup_event(struct wlan_objmgr_psoc *psoc,
				  uint32_t vdev_id,
				  WOW_WAKE_EVENT_TYPE wow_event);

/**
 * ucfg_pmo_disable_wakeup_event() -  disable wow wakeup events
 * @psoc: objmgr psoc
 * @vdev_id: vdev id
 * @wow_event: wow event to disable
 *
 * Return: none
 */
void ucfg_pmo_disable_wakeup_event(struct wlan_objmgr_psoc *psoc,
				   uint32_t vdev_id,
				   WOW_WAKE_EVENT_TYPE wow_event);

/**
 * ucfg_pmo_cache_arp_offload_req(): API to cache arp req in pmo vdev priv ctx
 * @arp_req: pmo arp req param
 *
 * Return QDF_STATUS -in case of success else return error
 */
QDF_STATUS ucfg_pmo_cache_arp_offload_req(struct pmo_arp_req *arp_req);

/**
 * ucfg_pmo_check_arp_offload(): API to check if arp offload cache/send is req
 * @psoc: objmgr psoc handle
 * @trigger: trigger reason
 * @vdev_id: vdev_id
 *
 * Return QDF_STATUS -in case of success else return error
 */
QDF_STATUS ucfg_pmo_check_arp_offload(struct wlan_objmgr_psoc *psoc,
				      enum pmo_offload_trigger trigger,
				      uint8_t vdev_id);

/**
 * ucfg_pmo_flush_arp_offload_req(): API to flush arp req from pmo vdev priv ctx
 * @vdev: objmgr vdev param
 *
 * Return QDF_STATUS -in case of success else return error
 */
QDF_STATUS ucfg_pmo_flush_arp_offload_req(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_pmo_enable_arp_offload_in_fwr(): API to enable arp req in fwr
 * @vdev: objmgr vdev param
 * @trigger: triger reason for enable arp offload
 *
 *  API to enable cache arp req in fwr from pmo vdev priv ctx
 *
 * Return QDF_STATUS -in case of success else return error
 */
QDF_STATUS
ucfg_pmo_enable_arp_offload_in_fwr(struct wlan_objmgr_vdev *vdev,
				   enum pmo_offload_trigger trigger);

/**
 * ucfg_pmo_disable_arp_offload_in_fwr(): API to disable arp req in fwr
 * @vdev: objmgr vdev param
 * @trigger: triger reason  for disable arp offload
 *  API to disable cache arp req in fwr
 *
 * Return QDF_STATUS -in case of success else return error
 */
QDF_STATUS
ucfg_pmo_disable_arp_offload_in_fwr(struct wlan_objmgr_vdev *vdev,
				    enum pmo_offload_trigger trigger);

/**
 * ucfg_pmo_get_arp_offload_params() - API to get arp offload params
 * @vdev: objmgr vdev
 * @params: output pointer to hold offload params
 *
 * Return: QDF_STATUS_SUCCESS in case of success else return error
 */
QDF_STATUS
ucfg_pmo_get_arp_offload_params(struct wlan_objmgr_vdev *vdev,
				struct pmo_arp_offload_params *params);

/**
 * ucfg_pmo_cache_ns_offload_req(): API to cache ns req in pmo vdev priv ctx
 * @ns_req: pmo ns req param
 *
 * Return QDF_STATUS -in case of success else return error
 */
QDF_STATUS ucfg_pmo_cache_ns_offload_req(struct pmo_ns_req *ns_req);

/**
 * ucfg_pmo_ns_offload_check(): API to check if offload cache/send is required
 * @psoc: pbjmgr psoc handle
 * @trigger: trigger reason to enable ns offload
 * @vdev_id: vdev id
 *
 * Return QDF_STATUS -in case of success else return error
 */
QDF_STATUS ucfg_pmo_ns_offload_check(struct wlan_objmgr_psoc *psoc,
				     enum pmo_offload_trigger trigger,
				     uint8_t vdev_id);

/**
 * ucfg_pmo_flush_ns_offload_req(): API to flush ns req from pmo vdev priv ctx
 * @vdev: vdev ojbmgr handle
 *
 * Return QDF_STATUS -in case of success else return error
 */
QDF_STATUS ucfg_pmo_flush_ns_offload_req(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_pmo_enable_ns_offload_in_fwr(): API to enable ns req in fwr
 * @arp_req: pmo arp req param
 * @trigger: trigger reason to enable ns offload
 *
 *  API to enable cache ns req in fwr from pmo vdev priv ctx
 *
 * Return QDF_STATUS -in case of success else return error
 */
QDF_STATUS
ucfg_pmo_enable_ns_offload_in_fwr(struct wlan_objmgr_vdev *vdev,
				  enum pmo_offload_trigger trigger);

/**
 * ucfg_pmo_disable_ns_offload_in_fwr(): API to disable ns req in fwr
 * @arp_req: pmo arp req param
 * @trigger: trigger reason to disable ns offload
 *
 *  API to disable ns req in fwr
 *
 * Return QDF_STATUS -in case of success else return error
 */
QDF_STATUS
ucfg_pmo_disable_ns_offload_in_fwr(struct wlan_objmgr_vdev *vdev,
				   enum pmo_offload_trigger trigger);

/**
 * ucfg_pmo_get_ns_offload_params() - API to get ns offload params
 * @vdev: objmgr vdev
 * @params: output pointer to hold offload params
 *
 * Return: QDF_STATUS_SUCCESS in case of success else return error
 */
QDF_STATUS
ucfg_pmo_get_ns_offload_params(struct wlan_objmgr_vdev *vdev,
			       struct pmo_ns_offload_params *params);

/**
 * ucfg_pmo_ns_addr_scope() - Convert linux specific IPv6 addr scope to
 *			      WLAN driver specific value
 * @scope: linux specific IPv6 addr scope
 *
 * Return: PMO identifier of linux IPv6 addr scope
 */
enum pmo_ns_addr_scope
ucfg_pmo_ns_addr_scope(uint32_t ipv6_scope);

/**
 * ucfg_pmo_enable_hw_filter_in_fwr() - enable previously configured hw filter
 * @vdev: objmgr vdev to configure
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_pmo_enable_hw_filter_in_fwr(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_pmo_enable_action_frame_patterns() - enable the action frame wake up
 * patterns as part of the enable host offloads.
 * @vdev: objmgr vdev to configure
 * @suspend_type: Suspend type. Runtime PM or System Suspend mode
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_pmo_enable_action_frame_patterns(struct wlan_objmgr_vdev *vdev,
				      enum qdf_suspend_type suspend_type);

/**
 * ucfg_pmo_disable_action_frame_patterns() - Reset the action frame wake up
 * patterns as a part of suspend resume.
 * @vdev: objmgr vdev to configure
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_pmo_disable_action_frame_patterns(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_pmo_disable_hw_filter_in_fwr() - disable previously configured hw filter
 * @vdev: objmgr vdev to configure
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_pmo_disable_hw_filter_in_fwr(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_pmo_max_mc_addr_supported() -  to get max support mc address
 * @psoc: objmgr psoc
 *
 * Return: max mc addr supported count for all vdev in corresponding psoc
 */
uint8_t ucfg_pmo_max_mc_addr_supported(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_cache_mc_addr_list(): API to cache mc addr list in pmo vdev priv obj
 * @psoc: objmgr psoc handle
 * @vdev_id: vdev id
 * @gtk_req: pmo gtk req param
 *
 * Return QDF_STATUS_SUCCESS -in case of success else return error
 */
QDF_STATUS ucfg_pmo_cache_mc_addr_list(
		struct pmo_mc_addr_list_params *mc_list_config);

/**
 * ucfg_pmo_flush_mc_addr_list(): API to flush mc addr list in pmo vdev priv obj
 * @psoc: objmgr psoc handle
 * @vdev_id: vdev id
 *
 * Return QDF_STATUS_SUCCESS -in case of success else return error
 */
QDF_STATUS ucfg_pmo_flush_mc_addr_list(struct wlan_objmgr_psoc *psoc,
				       uint8_t vdev_id);

/**
 * ucfg_pmo_enhance_mc_filter_enable() - enable enhanced multicast filtering
 * @vdev: the vdev to enable enhanced multicast filtering for
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
ucfg_pmo_enhanced_mc_filter_enable(struct wlan_objmgr_vdev *vdev)
{
	return pmo_core_enhanced_mc_filter_enable(vdev);
}

/**
 * ucfg_pmo_enhance_mc_filter_disable() - disable enhanced multicast filtering
 * @vdev: the vdev to disable enhanced multicast filtering for
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
ucfg_pmo_enhanced_mc_filter_disable(struct wlan_objmgr_vdev *vdev)
{
	return pmo_core_enhanced_mc_filter_disable(vdev);
}

/**
 * ucfg_pmo_enable_mc_addr_filtering_in_fwr(): Enable cached mc add list in fwr
 * @psoc: objmgr psoc handle
 * @vdev_id: vdev id
 * @gtk_req: pmo gtk req param
 * @action: true for enable els false
 *
 * API to enable cached mc add list in fwr
 *
 * Return QDF_STATUS_SUCCESS -in case of success else return error
 */
QDF_STATUS ucfg_pmo_enable_mc_addr_filtering_in_fwr(
		struct wlan_objmgr_psoc *psoc,
		uint8_t vdev_id,
		enum pmo_offload_trigger trigger);

/**
 * ucfg_pmo_disable_mc_addr_filtering_in_fwr(): Disable cached mc addr list
 * @psoc: objmgr psoc handle
 * @vdev_id: vdev id
 * @gtk_req: pmo gtk req param
 * @action: true for enable els false
 *
 * API to disable cached mc add list in fwr
 *
 * Return QDF_STATUS_SUCCESS -in case of success else return error
 */
QDF_STATUS ucfg_pmo_disable_mc_addr_filtering_in_fwr(
		struct wlan_objmgr_psoc *psoc,
		uint8_t vdev_id,
		enum pmo_offload_trigger trigger);

/**
 * ucfg_pmo_get_mc_addr_list() - API to get mc addr list configured
 * @psoc: objmgr psoc
 * @vdev_id: vdev identifier
 * @mc_list_req: output pointer to hold mc addr list params
 *
 * Return: QDF_STATUS_SUCCESS in case of success else return error
 */
QDF_STATUS
ucfg_pmo_get_mc_addr_list(struct wlan_objmgr_psoc *psoc,
			  uint8_t vdev_id,
			  struct pmo_mc_addr_list *mc_list_req);

/**
 * ucfg_pmo_cache_gtk_offload_req(): API to cache gtk req in pmo vdev priv obj
 * @vdev: objmgr vdev handle
 * @gtk_req: pmo gtk req param
 *
 * Return QDF_STATUS_SUCCESS -in case of success else return error
 */
QDF_STATUS ucfg_pmo_cache_gtk_offload_req(struct wlan_objmgr_vdev *vdev,
					  struct pmo_gtk_req *gtk_req);

/**
 * ucfg_pmo_flush_gtk_offload_req(): Flush saved gtk req from pmo vdev priv obj
 * @vdev: objmgr vdev handle
 *
 * Return QDF_STATUS_SUCCESS -in case of success else return error
 */
QDF_STATUS ucfg_pmo_flush_gtk_offload_req(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_pmo_enable_gtk_offload_in_fwr(): enable cached gtk request in fwr
 * @vdev: objmgr vdev handle
 *
 * Return QDF_STATUS_SUCCESS -in case of success else return error
 */
QDF_STATUS ucfg_pmo_enable_gtk_offload_in_fwr(struct wlan_objmgr_vdev *vdev);

#ifdef WLAN_FEATURE_BIG_DATA_STATS
/**
 * ucfg_pmo_enable_igmp_offload(): enable igmp request in fwr
 * @vdev: objmgr vdev handle
 * @pmo_igmp_req: struct pmo_igmp_offload_req
 *
 * Return QDF_STATUS_SUCCESS -in case of success else return error
 */
QDF_STATUS ucfg_pmo_enable_igmp_offload(
				struct wlan_objmgr_vdev *vdev,
				struct pmo_igmp_offload_req *pmo_igmp_req);
#else
static inline
QDF_STATUS ucfg_pmo_enable_igmp_offload(
				struct wlan_objmgr_vdev *vdev,
				struct pmo_igmp_offload_req *pmo_igmp_req)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * ucfg_pmo_disable_gtk_offload_in_fwr(): disable cached gtk request in fwr
 * @vdev: objmgr vdev handle
 *
 * Return QDF_STATUS_SUCCESS -in case of success else return error
 */
QDF_STATUS ucfg_pmo_disable_gtk_offload_in_fwr(struct wlan_objmgr_vdev *vdev);

#ifdef WLAN_FEATURE_PACKET_FILTERING
/**
 * ucfg_pmo_get_pkt_filter_bitmap() - get default packet filters bitmap
 * @psoc: the psoc to query
 *
 * Return: retrieve packet filter bitmap configuration
 */
uint8_t ucfg_pmo_get_pkt_filter_bitmap(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_get_num_packet_filters() - get the number of packet filters
 * @psoc: the psoc to query
 *
 * Return: number of packet filters
 */
uint32_t ucfg_pmo_get_num_packet_filters(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_set_pkt_filter() - Set packet filter
 * @psoc: objmgr psoc handle
 * @pmo_set_pkt_fltr_req: packet filter set param
 * @vdev_id: vdev id
 *
 * Return QDF_STATUS_SUCCESS -in case of success else return error
 */
QDF_STATUS
ucfg_pmo_set_pkt_filter(struct wlan_objmgr_psoc *psoc,
			struct pmo_rcv_pkt_fltr_cfg *pmo_set_pkt_fltr_req,
			uint8_t vdev_id);

/**
 * ucfg_pmo_clear_pkt_filter() - Clear packet filter
 * @psoc: objmgr psoc handle
 * @pmo_clr_pkt_fltr_param: packet filter clear param
 * @vdev_id: vdev id
 *
 * Return QDF_STATUS_SUCCESS -in case of success else return error
 */
QDF_STATUS ucfg_pmo_clear_pkt_filter(
	struct wlan_objmgr_psoc *psoc,
	struct pmo_rcv_pkt_fltr_clear_param *pmo_clr_pkt_fltr_param,
	uint8_t vdev_id);
#else
static inline uint8_t
ucfg_pmo_get_pkt_filter_bitmap(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline uint32_t
ucfg_pmo_get_num_packet_filters(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline QDF_STATUS
ucfg_pmo_set_pkt_filter(
		struct wlan_objmgr_psoc *psoc,
		struct pmo_rcv_pkt_fltr_cfg *pmo_set_pkt_fltr_req,
		uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_clear_pkt_filter(
		struct wlan_objmgr_psoc *psoc,
		struct pmo_rcv_pkt_fltr_clear_param *pmo_clr_pkt_fltr_param,
		uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * ucfg_pmo_get_wow_enable() - Get wow enable type
 * @psoc: pointer to psoc object
 *
 * Return: wow enable type
 */
enum pmo_wow_enable_type
ucfg_pmo_get_wow_enable(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_set_wow_enable() - Set wow enable type
 * @psoc: pointer to psoc object
 * @val: wow enalbe value
 *
 * Return: None
 */
void
ucfg_pmo_set_wow_enable(struct wlan_objmgr_psoc *psoc,
			enum pmo_wow_enable_type val);

/**
 * ucfg_pmo_get_gtk_rsp(): API to send gtk response request to fwr
 * @vdev: objmgr vdev handle
 * @gtk_rsp: pmo gtk response request
 *
 * This api will send gtk response request to fwr
 *
 * Return QDF_STATUS_SUCCESS -in case of success else return error
 */
QDF_STATUS
ucfg_pmo_get_gtk_rsp(struct wlan_objmgr_vdev *vdev,
		     struct pmo_gtk_rsp_req *gtk_rsp_req);

/**
 * ucfg_pmo_update_extscan_in_progress(): update extscan is in progress flags
 * @vdev: objmgr vdev handle
 * @value:true if extscan is in progress else false
 *
 * Return: TRUE/FALSE
 */
void ucfg_pmo_update_extscan_in_progress(struct wlan_objmgr_vdev *vdev,
					 bool value);

/**
 * ucfg_pmo_update_p2plo_in_progress(): update p2plo is in progress flags
 * @vdev: objmgr vdev handle
 * @value:true if p2plo is in progress else false
 *
 * Return: TRUE/FALSE
 */
void ucfg_pmo_update_p2plo_in_progress(struct wlan_objmgr_vdev *vdev,
				       bool value);

/**
 * ucfg_pmo_lphb_config_req() -  Handles lphb config request for psoc
 * @psoc: objmgr psoc handle
 * @lphb_req: low power heart beat request
 * @lphb_cb_ctx: Context which needs to pass to soif when lphb callback called
 * @callback: upon receiving of lphb indication from fwr call lphb callback
 *
 * Return: QDF status
 */
QDF_STATUS ucfg_pmo_lphb_config_req(struct wlan_objmgr_psoc *psoc,
				    struct pmo_lphb_req *lphb_req,
				    void *lphb_cb_ctx,
				    pmo_lphb_callback callback);

/**
 * ucfg_pmo_psoc_update_power_save_mode() - update power save mode
 * @vdev: objmgr vdev handle
 * @value:vdev power save mode
 *
 * Return: None
 */
void ucfg_pmo_psoc_update_power_save_mode(struct wlan_objmgr_psoc *psoc,
					  uint8_t value);

/**
 * ucfg_pmo_psoc_update_dp_handle() - update psoc data path handle
 * @psoc: objmgr psoc handle
 * @dp_hdl: psoc data path handle
 *
 * Return: None
 */
void ucfg_pmo_psoc_update_dp_handle(struct wlan_objmgr_psoc *psoc,
				    void *dp_hdl);

/**
 * ucfg_pmo_psoc_update_htc_handle() - update psoc htc layer handle
 * @psoc: objmgr psoc handle
 * @htc_handle: psoc host-to-tagret layer (htc) handle
 *
 * Return: None
 */
void ucfg_pmo_psoc_update_htc_handle(struct wlan_objmgr_psoc *psoc,
				     void *htc_handle);

/**
 * ucfg_pmo_psoc_set_hif_handle() - Set psoc hif layer handle
 * @psoc: objmgr psoc handle
 * @hif_handle: hif context handle
 *
 * Return: None
 */
void ucfg_pmo_psoc_set_hif_handle(struct wlan_objmgr_psoc *psoc,
				  void *hif_handle);

/**
 * ucfg_pmo_psoc_set_txrx_pdev_id() - Set psoc pdev txrx layer handle
 * @psoc: objmgr psoc handle
 * @txrx_pdev_id: txrx pdev identifier
 *
 * Return: None
 */
void ucfg_pmo_psoc_set_txrx_pdev_id(struct wlan_objmgr_psoc *psoc,
				    uint8_t txrx_pdev_id);

/**
 * ucfg_pmo_psoc_user_space_suspend_req() -  Handles user space suspend req
 * @psoc: objmgr psoc handle
 * @type: type of suspend
 *
 * Handles user space suspend indication for psoc
 *
 * Return: QDF status
 */
QDF_STATUS ucfg_pmo_psoc_user_space_suspend_req(struct wlan_objmgr_psoc *psoc,
						enum qdf_suspend_type type);

/**
 * ucfg_pmo_psoc_user_space_resume_req() -  Handles user space resume req
 * @psoc: objmgr psoc handle
 * @type: type of suspend from which resume needed
 *
 * Handles user space resume indication for psoc
 *
 * Return: QDF status
 */
QDF_STATUS ucfg_pmo_psoc_user_space_resume_req(struct wlan_objmgr_psoc *psoc,
					       enum qdf_suspend_type type);

/**
 * ucfg_pmo_suspend_all_components() -  Suspend all components
 * @psoc: objmgr psoc handle
 * @type: type of suspend
 *
 * Suspend all components registered to pmo
 *
 * Return: QDF status
 */
QDF_STATUS ucfg_pmo_suspend_all_components(struct wlan_objmgr_psoc *psoc,
					   enum qdf_suspend_type type);

/**
 * ucfg_pmo_resume_all_components() -  Resume all components
 * @psoc: objmgr psoc handle
 * @type: type of suspend from which resume needed
 *
 * Resume all components registered to pmo
 *
 * Return: QDF status
 */
QDF_STATUS ucfg_pmo_resume_all_components(struct wlan_objmgr_psoc *psoc,
					  enum qdf_suspend_type type);

/**
 * ucfg_pmo_psoc_bus_suspend_req(): handles bus suspend for psoc
 * @psoc: objmgr psoc
 * @type: is this suspend part of runtime suspend or system suspend?
 * @wow_params: collection of wow enable override parameters
 *
 * Bails if a scan is in progress.
 * Calls the appropriate handlers based on configuration and event.
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS ucfg_pmo_psoc_bus_suspend_req(
		struct wlan_objmgr_psoc *psoc,
		enum qdf_suspend_type type,
		struct pmo_wow_enable_params *wow_params);

#ifdef FEATURE_RUNTIME_PM
/**
 * ucfg_pmo_psoc_bus_runtime_suspend(): handles bus runtime suspend for psoc
 * @psoc: objmgr psoc
 * @pld_cb: callback to call link auto suspend
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS ucfg_pmo_psoc_bus_runtime_suspend(struct wlan_objmgr_psoc *psoc,
					     pmo_pld_auto_suspend_cb pld_cb);

/**
 * ucfg_pmo_psoc_bus_runtime_resume(): handles bus runtime resume for psoc
 * @psoc: objmgr psoc
 * @pld_cb: callback to call link auto resume
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS ucfg_pmo_psoc_bus_runtime_resume(struct wlan_objmgr_psoc *psoc,
					    pmo_pld_auto_resume_cb pld_cb);
#endif

/**
 * ucfg_pmo_psoc_suspend_target() -Send suspend target command
 * @psoc: objmgr psoc handle
 * @disable_target_intr: disable target interrupt
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS
ucfg_pmo_psoc_suspend_target(struct wlan_objmgr_psoc *psoc,
			     int disable_target_intr);

QDF_STATUS
ucfg_pmo_add_wow_user_pattern(struct wlan_objmgr_vdev *vdev,
			      struct pmo_wow_add_pattern *ptrn);

/**
 * ucfg_pmo_del_wow_pattern() - Delete WoWl patterns
 * @vdev: objmgr vdev
 *
 * Return:QDF_STATUS_SUCCESS on success else error code
 */
QDF_STATUS
ucfg_pmo_del_wow_pattern(struct wlan_objmgr_vdev *vdev);

QDF_STATUS
ucfg_pmo_del_wow_user_pattern(struct wlan_objmgr_vdev *vdev,
			      uint8_t pattern_id);

/**
 * ucfg_pmo_psoc_bus_resume() -handle bus resume request for psoc
 * @psoc: objmgr psoc handle
 * @type: is this suspend part of runtime suspend or system suspend?
 *
 * Return:QDF_STATUS_SUCCESS on success else error code
 */
QDF_STATUS ucfg_pmo_psoc_bus_resume_req(struct wlan_objmgr_psoc *psoc,
					enum qdf_suspend_type type);

/**
 * ucfg_pmo_get_wow_bus_suspend(): API to check if wow bus is suspended or not
 * @psoc: objmgr psoc handle
 *
 * Return: True if bus suspende else false
 */
bool ucfg_pmo_get_wow_bus_suspend(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_psoc_handle_initial_wake_up() - update initial wake up
 * @cb_ctx: objmgr psoc handle as void * due to htc layer is not aware psoc
 *
 * Return: None
 */
void ucfg_pmo_psoc_handle_initial_wake_up(void *cb_ctx);

/**
 * ucfg_pmo_psoc_is_target_wake_up_received() - Get initial wake up status
 * @psoc: objmgr psoc handle
 *
 * Return: 0 on success else error code
 */
int ucfg_pmo_psoc_is_target_wake_up_received(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_psoc_is_target_wake_up_received() - Clear initial wake up status
 * @psoc: objmgr psoc handle
 *
 * Return: 0 on success else error code
 */
int ucfg_pmo_psoc_clear_target_wake_up(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_psoc_target_suspend_acknowledge() - Clear initial wake up status
 * @psoc: objmgr psoc handle
 *
 * Return: None
 */
void ucfg_pmo_psoc_target_suspend_acknowledge(void *context, bool wow_nack);

/**
 * ucfg_pmo_psoc_wakeup_host_event_received() - got host wake up evennt from fwr
 * @psoc: objmgr psoc handle
 *
 * Return: None
 */
void ucfg_pmo_psoc_wakeup_host_event_received(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_config_listen_interval() - function to configure listen interval
 * @vdev: objmgr vdev
 * @listen_interval: new listen interval passed by user
 *
 * This function allows user to configure listen interval dynamically
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_pmo_config_listen_interval(struct wlan_objmgr_vdev *vdev,
					   uint32_t listen_interval);

/**
 * ucfg_pmo_config_modulated_dtim() - function to configure modulated dtim
 * @vdev: objmgr vdev handle
 * @param_value: New modulated dtim value passed by user
 *
 * This function configures the modulated dtim in firmware
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_pmo_config_modulated_dtim(struct wlan_objmgr_vdev *vdev,
					  uint32_t mod_dtim);

#ifdef WLAN_FEATURE_WOW_PULSE
/**
 * ucfg_pmo_is_wow_pulse_enabled() - to get wow pulse enable configuration
 * @psoc: objmgr psoc handle
 *
 * Return: wow pulse enable configuration
 */
bool ucfg_pmo_is_wow_pulse_enabled(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_get_wow_pulse_pin() - to get wow pulse pin configuration
 * @psoc: objmgr psoc handle
 *
 * Return: wow pulse pin configuration
 */
uint8_t ucfg_pmo_get_wow_pulse_pin(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_get_wow_pulse_interval_high() - to get wow pulse interval high
 * @psoc: objmgr psoc handle
 *
 * Return: wow pulse interval high configuration
 */
uint16_t ucfg_pmo_get_wow_pulse_interval_high(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_get_wow_pulse_interval_low() - to get wow pulse interval low
 * @psoc: objmgr psoc handle
 *
 * Return: wow pulse interval high configuration
 */
uint16_t ucfg_pmo_get_wow_pulse_interval_low(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_get_wow_pulse_repeat_count() - to get wow pulse repeat count
 * @psoc: objmgr psoc handle
 *
 * Return: wow pulse repeat count configuration
 */
uint32_t ucfg_pmo_get_wow_pulse_repeat_count(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_get_wow_pulse_init_state() - to get wow pulse init state
 * @psoc: objmgr psoc handle
 *
 * Return: wow pulse init state configuration
 */
uint32_t ucfg_pmo_get_wow_pulse_init_state(struct wlan_objmgr_psoc *psoc);
#else
static inline bool
ucfg_pmo_is_wow_pulse_enabled(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline uint8_t
ucfg_pmo_get_wow_pulse_pin(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline uint16_t
ucfg_pmo_get_wow_pulse_interval_high(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline uint32_t
ucfg_pmo_get_wow_pulse_repeat_count(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline uint32_t
ucfg_pmo_get_wow_pulse_init_state(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}
#endif

/**
 * ucfg_pmo_is_active_mode_offloaded() - get active mode offload configuration
 * @psoc: objmgr psoc handle
 *
 * Return: retrieve active mode offload configuration
 */
bool ucfg_pmo_is_active_mode_offloaded(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_get_auto_power_fail_mode() - to get auto power save failure mode
 * @psoc: objmgr psoc handle
 *
 * Return: auto power save failure mode configuration
 */
enum pmo_auto_pwr_detect_failure_mode
ucfg_pmo_get_auto_power_fail_mode(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_set_wow_data_inactivity_timeout() - Set wow data inactivity timeout
 * @psoc: pointer to psoc object
 * @val: wow data inactivity timeout value
 *
 * Return: None
 */
void
ucfg_pmo_set_wow_data_inactivity_timeout(struct wlan_objmgr_psoc *psoc,
					 uint8_t val);

/**
 * ucfg_pmo_is_pkt_filter_enabled() - pmo packet filter feature enable or not
 * @psoc: objmgr psoc handle
 *
 * Return: pmo packet filter feature enable/disable
 */
bool ucfg_pmo_is_pkt_filter_enabled(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_get_active_uc_apf_mode() - to get the modes active APF
 * for MC/BC packets
 * @psoc: objmgr psoc handle
 *
 * Return: the modes active APF
 */
enum active_apf_mode
ucfg_pmo_get_active_uc_apf_mode(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_get_active_mc_bc_apf_mode() - to get the modes active APF
 * for uc packets
 * @psoc: objmgr psoc handle
 *
 * Return: the modes active APF
 */
enum active_apf_mode
ucfg_pmo_get_active_mc_bc_apf_mode(struct wlan_objmgr_psoc *psoc);
#ifdef FEATURE_WLAN_APF
/**
 * ucfg_pmo_is_apf_enabled() - to get apf configuration
 * @psoc: objmgr psoc handle
 *
 * Return: true if enabled, it is intersection of ini and target cap
 */
bool ucfg_pmo_is_apf_enabled(struct wlan_objmgr_psoc *psoc);
#else
static inline bool ucfg_pmo_is_apf_enabled(struct wlan_objmgr_psoc *psoc)
{
	return false;
}
#endif

/**
 * ucfg_pmo_core_txrx_suspend(): suspends TX/RX
 * @psoc: objmgr psoc
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS ucfg_pmo_core_txrx_suspend(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_core_txrx_resume(): resumes TX/RX
 * @psoc: objmgr psoc
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS ucfg_pmo_core_txrx_resume(struct wlan_objmgr_psoc *psoc);
#else /* WLAN_POWER_MANAGEMENT_OFFLOAD */
static inline QDF_STATUS
ucfg_pmo_psoc_open(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_psoc_close(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline uint32_t
ucfg_pmo_get_apf_instruction_size(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline uint8_t
ucfg_pmo_get_pkt_filter_bitmap(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline uint32_t
ucfg_pmo_get_num_packet_filters(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline uint8_t
ucfg_pmo_get_num_wow_filters(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline QDF_STATUS
ucfg_pmo_get_psoc_config(
		struct wlan_objmgr_psoc *psoc,
		struct pmo_psoc_cfg *psoc_cfg)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_update_psoc_config(
		struct wlan_objmgr_psoc *psoc,
		struct pmo_psoc_cfg *psoc_cfg)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_psoc_set_caps(
		struct wlan_objmgr_psoc *psoc,
		struct pmo_device_caps *caps)
{
	return QDF_STATUS_SUCCESS;
}

static inline bool
ucfg_pmo_is_ap_mode_supports_arp_ns(
		struct wlan_objmgr_psoc *psoc,
		enum QDF_OPMODE vdev_opmode)
{
	return true;
}

static inline bool
ucfg_pmo_is_vdev_connected(struct wlan_objmgr_vdev *vdev)
{
	return true;
}

static inline bool
ucfg_pmo_is_vdev_supports_offload(struct wlan_objmgr_vdev *vdev)
{
	return true;
}

static inline void
ucfg_pmo_enable_wakeup_event(
		struct wlan_objmgr_psoc *psoc,
		uint32_t vdev_id, uint32_t *bitmap)
{
}

static inline void
ucfg_pmo_disable_wakeup_event(
		struct wlan_objmgr_psoc *psoc,
		uint32_t vdev_id, uint32_t bitmap)
{
}

static inline QDF_STATUS
ucfg_pmo_cache_arp_offload_req(struct pmo_arp_req *arp_req)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS ucfg_pmo_check_arp_offload(struct wlan_objmgr_psoc *psoc,
				      enum pmo_offload_trigger trigger,
				      uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_flush_arp_offload_req(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_enable_arp_offload_in_fwr(
		struct wlan_objmgr_vdev *vdev,
		enum pmo_offload_trigger trigger)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_disable_arp_offload_in_fwr(
		struct wlan_objmgr_vdev *vdev,
		enum pmo_offload_trigger trigger)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_get_arp_offload_params(struct wlan_objmgr_vdev *vdev,
				struct pmo_arp_offload_params *params)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_cache_ns_offload_req(struct pmo_ns_req *ns_req)
{
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_pmo_ns_offload_check(struct wlan_objmgr_psoc *psoc,
				     enum pmo_offload_trigger trigger,
				     uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_flush_ns_offload_req(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_enable_ns_offload_in_fwr(
		struct wlan_objmgr_vdev *vdev,
		enum pmo_offload_trigger trigger)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_disable_ns_offload_in_fwr(
		struct wlan_objmgr_vdev *vdev,
		enum pmo_offload_trigger trigger)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_get_ns_offload_params(struct wlan_objmgr_vdev *vdev,
			       struct pmo_ns_offload_params *params)
{
	return QDF_STATUS_SUCCESS;
}

static inline enum pmo_ns_addr_scope
ucfg_pmo_ns_addr_scope(uint32_t ipv6_scope)
{
	return PMO_NS_ADDR_SCOPE_INVALID;
}

static inline QDF_STATUS
ucfg_pmo_cache_mc_addr_list(
		struct pmo_mc_addr_list_params *mc_list_config)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_flush_mc_addr_list(
		struct wlan_objmgr_psoc *psoc,
		uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_enable_mc_addr_filtering_in_fwr(
		struct wlan_objmgr_psoc *psoc,
		uint8_t vdev_id,
		enum pmo_offload_trigger trigger)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_disable_mc_addr_filtering_in_fwr(
		struct wlan_objmgr_psoc *psoc,
		uint8_t vdev_id,
		enum pmo_offload_trigger trigger)
{
	return QDF_STATUS_SUCCESS;
}

static inline uint8_t
ucfg_pmo_max_mc_addr_supported(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline QDF_STATUS
ucfg_pmo_get_mc_addr_list(struct wlan_objmgr_psoc *psoc,
			  uint8_t vdev_id,
			  struct pmo_mc_addr_list *mc_list_req)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_cache_gtk_offload_req(
		struct wlan_objmgr_vdev *vdev,
		struct pmo_gtk_req *gtk_req)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_flush_gtk_offload_req(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_enable_gtk_offload_in_fwr(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_enable_igmp_offload(struct wlan_objmgr_vdev *vdev,
			     struct pmo_igmp_offload_req *pmo_igmp_req)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
ucfg_pmo_disable_gtk_offload_in_fwr(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_set_pkt_filter(
		struct wlan_objmgr_psoc *psoc,
		struct pmo_rcv_pkt_fltr_cfg *pmo_set_pkt_fltr_req,
		uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_clear_pkt_filter(
		struct wlan_objmgr_psoc *psoc,
		struct pmo_rcv_pkt_fltr_clear_param *pmo_clr_pkt_fltr_param,
		uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_get_gtk_rsp(
		struct wlan_objmgr_vdev *vdev,
		struct pmo_gtk_rsp_req *gtk_rsp_req)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
ucfg_pmo_update_extscan_in_progress(
		struct wlan_objmgr_vdev *vdev,
		bool value)
{
}

static inline void
ucfg_pmo_update_p2plo_in_progress(
		struct wlan_objmgr_vdev *vdev,
		bool value)
{
}

static inline QDF_STATUS
ucfg_pmo_lphb_config_req(
		struct wlan_objmgr_psoc *psoc,
		struct pmo_lphb_req *lphb_req, void *lphb_cb_ctx,
		pmo_lphb_callback callback)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
ucfg_pmo_psoc_update_power_save_mode(
		struct wlan_objmgr_psoc *psoc,
		uint8_t value)
{
}

static inline void
ucfg_pmo_psoc_update_dp_handle(
		struct wlan_objmgr_psoc *psoc,
		void *dp_handle)
{
}

static inline void
ucfg_pmo_psoc_update_htc_handle(
		struct wlan_objmgr_psoc *psoc,
		void *htc_handle)
{
}

static inline void
ucfg_pmo_psoc_set_hif_handle(
		struct wlan_objmgr_psoc *psoc,
		void *hif_handle)
{
}

static inline void
ucfg_pmo_psoc_set_txrx_pdev_id(
		struct wlan_objmgr_psoc *psoc,
		uint8_t txrx_pdev_id)
{
}

static inline void
ucfg_pmo_psoc_handle_initial_wake_up(void *cb_ctx)
{
}

static inline QDF_STATUS
ucfg_pmo_psoc_user_space_suspend_req(
		struct wlan_objmgr_psoc *psoc,
		enum qdf_suspend_type type)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_psoc_user_space_resume_req(
		struct wlan_objmgr_psoc *psoc,
		enum qdf_suspend_type type)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_suspend_all_components(struct wlan_objmgr_psoc *psoc,
				enum qdf_suspend_type type)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_resume_all_components(struct wlan_objmgr_psoc *psoc,
			       enum qdf_suspend_type type)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_psoc_bus_suspend_req(
		struct wlan_objmgr_psoc *psoc,
		enum qdf_suspend_type type,
		struct pmo_wow_enable_params *wow_params)
{
	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_RUNTIME_PM
static inline QDF_STATUS
ucfg_pmo_psoc_bus_runtime_suspend(
		struct wlan_objmgr_psoc *psoc,
		pmo_pld_auto_suspend_cb pld_cb)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_psoc_bus_runtime_resume(
		struct wlan_objmgr_psoc *psoc,
		pmo_pld_auto_suspend_cb pld_cb)
{
	return QDF_STATUS_SUCCESS;
}
#endif

static inline QDF_STATUS
ucfg_pmo_psoc_suspend_target(
		struct wlan_objmgr_psoc *psoc,
		int disable_target_intr)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_add_wow_user_pattern(
		struct wlan_objmgr_vdev *vdev,
		struct pmo_wow_add_pattern *ptrn)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_del_wow_user_pattern(
		struct wlan_objmgr_vdev *vdev,
		uint8_t pattern_id)
{
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_pmo_del_wow_pattern(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_psoc_bus_resume_req(
		struct wlan_objmgr_psoc *psoc,
		enum qdf_suspend_type type)
{
	return QDF_STATUS_SUCCESS;
}

static inline bool
ucfg_pmo_get_wow_bus_suspend(struct wlan_objmgr_psoc *psoc)
{
	return true;
}

static inline int
ucfg_pmo_psoc_is_target_wake_up_received(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline int
ucfg_pmo_psoc_clear_target_wake_up(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline void
ucfg_pmo_psoc_target_suspend_acknowledge(void *context, bool wow_nack)
{
}

static inline void
ucfg_pmo_psoc_wakeup_host_event_received(struct wlan_objmgr_psoc *psoc)
{
}

static inline QDF_STATUS
ucfg_pmo_enable_hw_filter_in_fwr(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_disable_hw_filter_in_fwr(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_enhanced_mc_filter_enable(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_enhanced_mc_filter_disable(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_config_listen_interval(struct wlan_objmgr_vdev *vdev,
				uint32_t listen_interval)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_pmo_config_modulated_dtim(struct wlan_objmgr_vdev *vdev,
			       uint32_t mod_dtim)
{
	return QDF_STATUS_SUCCESS;
}

static inline bool
ucfg_pmo_is_arp_offload_enabled(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline bool
ucfg_pmo_is_igmp_offload_enabled(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline void
ucfg_pmo_set_arp_offload_enabled(struct wlan_objmgr_psoc *psoc,
				 bool val)
{
}

static inline void
ucfg_pmo_set_igmp_offload_enabled(struct wlan_objmgr_psoc *psoc,
				  bool val)
{
}

static inline bool
ucfg_pmo_is_wow_pulse_enabled(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline uint8_t
ucfg_pmo_get_wow_pulse_pin(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline uint16_t
ucfg_pmo_get_wow_pulse_interval_high(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline uint16_t
ucfg_pmo_get_wow_pulse_interval_low(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline bool
ucfg_pmo_is_active_mode_offloaded(struct wlan_objmgr_psoc *psoc)
{
	return true;
}

static inline enum pmo_auto_pwr_detect_failure_mode
ucfg_pmo_get_auto_power_fail_mode(struct wlan_objmgr_psoc *psoc)
{
	return PMO_FW_TO_CRASH_ON_PWR_FAILURE;
}

static inline bool ucfg_pmo_is_apf_enabled(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline bool ucfg_pmo_is_ssdp_enabled(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline bool ucfg_pmo_is_ns_offloaded(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline uint8_t
ucfg_pmo_get_sta_dynamic_dtim(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline uint8_t
ucfg_pmo_get_sta_mod_dtim(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline void
ucfg_pmo_set_sta_mod_dtim(struct wlan_objmgr_psoc *psoc,
			  uint8_t val)
{
}

static inline bool
ucfg_pmo_is_mc_addr_list_enabled(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline enum powersave_mode
ucfg_pmo_get_power_save_mode(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline enum powersave_mode
ucfg_pmo_get_default_power_save_mode(struct wlan_objmgr_psoc *psoc)
{
	return PMO_PS_ADVANCED_POWER_SAVE_DISABLE;
}

static inline void
ucfg_pmo_set_power_save_mode(struct wlan_objmgr_psoc *psoc,
			     enum powersave_mode val)
{
}

static inline uint8_t
ucfg_pmo_get_max_ps_poll(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline uint8_t
ucfg_pmo_power_save_offload_enabled(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline void
ucfg_pmo_set_wow_data_inactivity_timeout(struct wlan_objmgr_psoc *psoc,
					 uint8_t val)
{
}

static inline bool
ucfg_pmo_is_pkt_filter_enabled(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

enum active_apf_mode
ucfg_pmo_get_active_uc_apf_mode(struct wlan_objmgr_psoc *psoc);
{
	return 0;
}

enum active_apf_mode
ucfg_pmo_get_active_mc_bc_apf_mode(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

QDF_STATUS ucfg_pmo_core_txrx_suspend(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_pmo_core_txrx_resume(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_POWER_MANAGEMENT_OFFLOAD */

#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
/**
 * ucfg_pmo_extwow_is_goto_suspend_enabled() - Get extwow goto suspend enable
 * @psoc: pointer to psoc object
 *
 * Return: extend wow goto suspend enable or not
 */
bool
ucfg_pmo_extwow_is_goto_suspend_enabled(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_extwow_app1_wakeup_pin_num() - Get wakeup1 PIN number
 * @psoc: pointer to psoc object
 *
 * Return: wakeup1 PIN number
 */
uint8_t
ucfg_pmo_extwow_app1_wakeup_pin_num(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_extwow_app2_wakeup_pin_num() - Get wakeup2 PIN number
 * @psoc: pointer to psoc object
 *
 * Return: wakeup2 PIN number
 */
uint8_t
ucfg_pmo_extwow_app2_wakeup_pin_num(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_extwow_app2_init_ping_interval() - Get keep alive init ping interval
 * @psoc: pointer to psoc object
 *
 * Return: keep alive init ping interval
 */
uint32_t
ucfg_pmo_extwow_app2_init_ping_interval(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_extwow_app2_min_ping_interval() - Get keep alive min ping interval
 * @psoc: pointer to psoc object
 *
 * Return: keep alive min ping interval
 */
uint32_t
ucfg_pmo_extwow_app2_min_ping_interval(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_extwow_app2_max_ping_interval() - Get keep alive max ping interval
 * @psoc: pointer to psoc object
 *
 * Return: keep alive max ping interval
 */
uint32_t
ucfg_pmo_extwow_app2_max_ping_interval(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_extwow_app2_inc_ping_interval() - Get keep alive inc ping interval
 * @psoc: pointer to psoc object
 *
 * Return: keep alive inc ping interval
 */
uint32_t
ucfg_pmo_extwow_app2_inc_ping_interval(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_extwow_app2_tcp_src_port() - Get TCP source port
 * @psoc: pointer to psoc object
 *
 * Return: TCP source port
 */
uint16_t
ucfg_pmo_extwow_app2_tcp_src_port(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_extwow_app2_tcp_dst_port() - Get TCP Destination port
 * @psoc: pointer to psoc object
 *
 * Return: TCP Destination port
 */
uint16_t
ucfg_pmo_extwow_app2_tcp_dst_port(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_extwow_app2_tcp_tx_timeout() - Get TCP Tx timeout
 * @psoc: pointer to psoc object
 *
 * Return: TCP Tx timeout
 */
uint32_t
ucfg_pmo_extwow_app2_tcp_tx_timeout(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_extwow_app2_tcp_rx_timeout() - to get extwow tcp rx timeout
 * @psoc: objmgr psoc handle
 *
 * Return: retrieve extwow app2 tcp rx timeout configuration
 */
uint32_t
ucfg_pmo_extwow_app2_tcp_rx_timeout(struct wlan_objmgr_psoc *psoc);

#else
static inline bool
ucfg_pmo_extwow_is_goto_suspend_enabled(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline uint32_t
ucfg_pmo_extwow_app1_wakeup_pin_num(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline uint32_t
ucfg_pmo_extwow_app2_wakeup_pin_num(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline uint32_t
ucfg_pmo_extwow_app2_init_ping_interval(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline uint32_t
ucfg_pmo_extwow_app2_min_ping_interval(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline uint32_t
ucfg_pmo_extwow_app2_max_ping_interval(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline uint32_t
ucfg_pmo_extwow_app2_inc_ping_interval(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline uint16_t
ucfg_pmo_extwow_app2_tcp_src_port(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline uint16_t
ucfg_pmo_extwow_app2_tcp_dst_port(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline uint32_t
ucfg_pmo_extwow_app2_tcp_tx_timeout(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline uint32_t
ucfg_pmo_extwow_app2_tcp_rx_timeout(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}
#endif

#ifdef FEATURE_RUNTIME_PM
/**
 * ucfg_pmo_get_runtime_pm_delay() - Get runtime pm's inactivity timer
 * @psoc: pointer to psoc object
 *
 * Return: runtime pm's inactivity timer
 */
uint32_t
ucfg_pmo_get_runtime_pm_delay(struct wlan_objmgr_psoc *psoc);
#else
static inline uint32_t
ucfg_pmo_get_runtime_pm_delay(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}
#endif /* FEATURE_RUNTIME_PM */

/**
 * ucfg_pmo_get_enable_sap_suspend - Return enable_sap_suspend value to caller
 * @psoc: Pointer to psoc object
 *
 * Return: The value of enable_sap_suspend as stored in CFG
 */
bool
ucfg_pmo_get_enable_sap_suspend(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_get_sap_mode_bus_suspend() - get PMO config for PCIe bus
 * suspend in SAP mode with one or more clients
 * @psoc: pointer to psoc object
 *
 * Return: bool
 */
bool
ucfg_pmo_get_sap_mode_bus_suspend(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pmo_get_go_mode_bus_suspend() - get PMO config for PCIe bus
 * suspend in P2PGO mode with one or more clients
 * @psoc: pointer to psoc object
 *
 * Return: bool
 */
bool
ucfg_pmo_get_go_mode_bus_suspend(struct wlan_objmgr_psoc *psoc);

#ifdef SYSTEM_PM_CHECK
/**
 * ucfg_pmo_notify_system_resume() - system resume notification to pmo
 * @psoc: pointer to psoc object
 *
 * Return: None
 */
void
ucfg_pmo_notify_system_resume(struct wlan_objmgr_psoc *psoc);
#else
static inline
void ucfg_pmo_notify_system_resume(struct wlan_objmgr_psoc *psoc)
{
}
#endif
#endif /* end  of _WLAN_PMO_UCFG_API_H_ */
