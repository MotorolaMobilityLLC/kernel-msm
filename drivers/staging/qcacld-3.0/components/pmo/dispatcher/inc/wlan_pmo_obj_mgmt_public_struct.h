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
 * DOC: Declare various struct, macros which are used for object mgmt in pmo.
 *
 * Note: This file shall not contain public API's prototype/declarations.
 *
 */

#ifndef _WLAN_PMO_OBJ_MGMT_PUBLIC_STRUCT_H_
#define _WLAN_PMO_OBJ_MGMT_PUBLIC_STRUCT_H_

#include "wlan_pmo_common_public_struct.h"
#include "wlan_pmo_arp_public_struct.h"
#include "wlan_pmo_ns_public_struct.h"
#include "wlan_pmo_gtk_public_struct.h"
#include "wlan_pmo_wow_public_struct.h"
#include "wlan_pmo_mc_addr_filtering_public_struct.h"
#include "wlan_pmo_hw_filter_public_struct.h"
#include "wlan_pmo_pkt_filter_public_struct.h"
#include "wlan_pmo_lphb_public_struct.h"

/**
 * typedef for vdev notifying the vdev pause bitmap new value to mlme
 */
typedef void (*pmo_notify_pause_bitmap)(uint8_t vdev_id, uint16_t value);

/**
 * typedef for function that gets cfg integer from mlme
 */
typedef QDF_STATUS (*pmo_get_cfg_int)(int cfg_id, int *value);

/**
 * typedef for function that gets dtim period from mlme
 */
typedef QDF_STATUS (*pmo_get_dtim_period)(uint8_t vdev_id, uint8_t *value);

/**
 * typedef for function that gets  beacon interval from mlme
 */
typedef QDF_STATUS (*pmo_get_beacon_interval)(uint8_t vdev_id, uint16_t *value);

/**
 * typedef for getting vdev pause bitmap
 */
typedef  uint16_t(*pmo_get_pause_bitmap)(uint8_t vdev_id);

/**
 * typedef for getting vdev datapath handle
 */
typedef struct cdp_vdev * (*pmo_get_vdev_dp_handle)(uint8_t vdev_id);

/**
 * typedef to know is deviec is in power save mode
 */
typedef  bool (*pmo_is_device_in_low_pwr_mode)(uint8_t vdev_id);

/*
 * typedef for pld auto suspend callback during runtime suspend
 */
typedef int (*pmo_pld_auto_suspend_cb)(void);

/*
 * typedef for pld auto resume callback during runtime resume
 */
typedef int (*pmo_pld_auto_resume_cb)(void);

/**
 * struct wlan_pmo_tx_ops - structure of tx function
 *					pointers for pmo component
 * @send_arp_offload_req: fp to send arp offload request
 * @send_ns_offload_req: fp to send ns offload request
 * @send_non_arp_bcast_filter_req: for enable/disable  broadcast filter
 * @send_set_pkt_filter: send set packet filter
 * @send_clear_pkt_filter: send clear packet filter
 * @send_enable_wakeup_event_req: fp to send enable wow wakeup events req
 * @send_disable_wakeup_event_req: fp to send disable wow wakeup events req
 * @send_add_wow_pattern: fp to send wow pattern request
 * @del_wow_pattern: fp to delete wow pattern from firmware
 * @send_enhance_mc_offload_req: fp to send enhanced multicast offload request
 * @send_set_mc_filter_req: fp to send set mc filter request
 * @send_clear_mc_filter_req: fp to send clear mc filter request
 * @get_multiple_mc_filter_support: fp to get mc filter support
 * @send_set_multiple_mc_filter_req: fp to send set multiple mc filter request
 * @send_clear_multiple_mc_filter_req: fp to send clear multiple mc filter req
 * @send_ra_filter_req: fp to send ra filter request
 * @send_gtk_offload_req: fp to send gtk offload request command
 * @send_get_gtk_rsp_cmd: fp to send get gtk response request cmd to firmware
 * @send_action_frame_pattern_req: fp to send wow action frame patterns request
 * @send_lphb_enable: fp to send lphb enable request command
 * @send_lphb_tcp_params: fp to send lphb tcp params request command
 * @send_lphb_tcp_filter_req: fp to send lphb tcp packet filter request command
 * @send_lphb_upd_params: fp to send lphb udp params request command
 * @send_lphb_udp_filter_req: fp to send lphb udp packet filter request command
 * @send_vdev_param_update_req: fp to send vdev param request
 * @send_vdev_set_sta_ps_param: fp to send sta vdev ps power set req
 * @psoc_update_wow_bus_suspend: fp to update bus suspend req flag at wmi
 * @psoc_get_host_credits: fp to get the host credits
 * @psoc_get_pending_cmnds: fp to get the host pending wmi commands
 * @update_target_suspend_flag: fp to update target suspend flag at wmi
 * @psoc_send_wow_enable_req: fp to send wow enable request
 * @psoc_send_supend_req: fp to send target suspend request
 * @psoc_set_runtime_pm_in_progress: fp to set runtime pm is in progress status
 * @psoc_get_runtime_pm_in_progress: fp to get runtime pm is in progress status
 * @psoc_send_host_wakeup_ind: fp tp send host wake indication to fwr
 * @psoc_send_target_resume_req: fp to send target resume request
 * @psoc_send_d0wow_enable_req: fp to send D0 WOW enable request
 * @psoc_send_d0wow_disable_req: fp to send D0 WOW disable request
 * @psoc_send_idle_roam_suspend_mode: fp to send suspend mode for
 * idle roam  trigger to firmware.
 */
struct wlan_pmo_tx_ops {
	QDF_STATUS (*send_arp_offload_req)(struct wlan_objmgr_vdev *vdev,
			struct pmo_arp_offload_params *arp_offload_req,
			struct pmo_ns_offload_params *ns_offload_req);
	QDF_STATUS (*send_conf_hw_filter_req)(
			struct wlan_objmgr_psoc *psoc,
			struct pmo_hw_filter_params *req);
	QDF_STATUS (*send_ns_offload_req)(struct wlan_objmgr_vdev *vdev,
			struct pmo_arp_offload_params *arp_offload_req,
			struct pmo_ns_offload_params *ns_offload_req);
#ifdef WLAN_FEATURE_PACKET_FILTERING
	QDF_STATUS(*send_set_pkt_filter)(struct wlan_objmgr_vdev *vdev,
			struct pmo_rcv_pkt_fltr_cfg *pmo_set_pkt_fltr_req);
	QDF_STATUS(*send_clear_pkt_filter)(struct wlan_objmgr_vdev *vdev,
			struct pmo_rcv_pkt_fltr_clear_param
						*pmo_clr_pkt_fltr_param);
#endif
	QDF_STATUS (*send_enable_wow_wakeup_event_req)(
			struct wlan_objmgr_vdev *vdev,
			uint32_t *bitmap);
	QDF_STATUS (*send_disable_wow_wakeup_event_req)(
			struct wlan_objmgr_vdev *vdev,
			uint32_t *bitmap);
	QDF_STATUS (*send_add_wow_pattern)(
			struct wlan_objmgr_vdev *vdev,
			uint8_t ptrn_id, const uint8_t *ptrn, uint8_t ptrn_len,
			uint8_t ptrn_offset, const uint8_t *mask,
			uint8_t mask_len, bool user);
	QDF_STATUS (*del_wow_pattern)(
			struct wlan_objmgr_vdev *vdev, uint8_t ptrn_id);
	QDF_STATUS (*send_enhance_mc_offload_req)(
			struct wlan_objmgr_vdev *vdev, bool enable);
	QDF_STATUS (*send_set_mc_filter_req)(
			struct wlan_objmgr_vdev *vdev,
			struct qdf_mac_addr multicast_addr);
	QDF_STATUS (*send_clear_mc_filter_req)(
			struct wlan_objmgr_vdev *vdev,
			struct qdf_mac_addr multicast_addr);
	bool (*get_multiple_mc_filter_support)(
			struct wlan_objmgr_psoc *psoc);
	QDF_STATUS(*send_set_multiple_mc_filter_req)(
			struct wlan_objmgr_vdev *vdev,
			struct pmo_mc_addr_list *mc_list);
	QDF_STATUS(*send_clear_multiple_mc_filter_req)(
			struct wlan_objmgr_vdev *vdev,
			struct pmo_mc_addr_list *mc_list);
	QDF_STATUS (*send_ra_filter_req)(
			struct wlan_objmgr_vdev *vdev,
			uint8_t default_pattern, uint16_t rate_limit_interval);
	QDF_STATUS (*send_gtk_offload_req)(
			struct wlan_objmgr_vdev *vdev,
			struct pmo_gtk_req *gtk_offload_req);
	QDF_STATUS (*send_get_gtk_rsp_cmd)(struct wlan_objmgr_vdev *vdev);
	QDF_STATUS (*send_action_frame_pattern_req)(
			struct wlan_objmgr_vdev *vdev,
			struct pmo_action_wakeup_set_params *ip_cmd);
	QDF_STATUS (*send_lphb_enable)(
			struct wlan_objmgr_psoc *psoc,
			struct pmo_lphb_enable_req *ts_lphb_enable);
	QDF_STATUS (*send_lphb_tcp_params)(
			struct wlan_objmgr_psoc *psoc,
			struct pmo_lphb_tcp_params *ts_lphb_tcp_param);
	QDF_STATUS (*send_lphb_tcp_filter_req)(
			struct wlan_objmgr_psoc *psoc,
			struct pmo_lphb_tcp_filter_req *ts_lphb_tcp_filter);
	QDF_STATUS (*send_lphb_upd_params)(
			struct wlan_objmgr_psoc *psoc,
			struct pmo_lphb_udp_params *ts_lphb_udp_param);
	QDF_STATUS (*send_lphb_udp_filter_req)(
			struct wlan_objmgr_psoc *psoc,
			struct pmo_lphb_udp_filter_req *ts_lphb_udp_filter);
	QDF_STATUS (*send_vdev_param_update_req)(
			struct wlan_objmgr_vdev *vdev,
			uint32_t param_id, uint32_t param_value);
	QDF_STATUS (*send_vdev_sta_ps_param_req)(
			struct wlan_objmgr_vdev *vdev,
			uint32_t ps_mode, uint32_t value);
#ifdef WLAN_FEATURE_IGMP_OFFLOAD
	QDF_STATUS (*send_igmp_offload_req)(
			struct wlan_objmgr_vdev *vdev,
			struct pmo_igmp_offload_req *pmo_igmp_req);
#endif
	void (*psoc_update_wow_bus_suspend)(
			struct wlan_objmgr_psoc *psoc, uint8_t value);
	int (*psoc_get_host_credits)(
			struct wlan_objmgr_psoc *psoc);
	int (*psoc_get_pending_cmnds)(
			struct wlan_objmgr_psoc *psoc);
	void (*update_target_suspend_flag)(
		struct wlan_objmgr_psoc *psoc, uint8_t value);
	void (*update_target_suspend_acked_flag)(
		struct wlan_objmgr_psoc *psoc, uint8_t value);
	bool (*is_target_suspended)(struct wlan_objmgr_psoc *psoc);
	QDF_STATUS (*psoc_send_wow_enable_req)(struct wlan_objmgr_psoc *psoc,
		struct pmo_wow_cmd_params *param);
	QDF_STATUS (*psoc_send_supend_req)(struct wlan_objmgr_psoc *psoc,
		struct pmo_suspend_params *param);
	void (*psoc_set_runtime_pm_in_progress)(struct wlan_objmgr_psoc *psoc,
						bool value);
	bool (*psoc_get_runtime_pm_in_progress)(struct wlan_objmgr_psoc *psoc);
	QDF_STATUS (*psoc_send_host_wakeup_ind)(struct wlan_objmgr_psoc *psoc);
	QDF_STATUS (*psoc_send_target_resume_req)(
			struct wlan_objmgr_psoc *psoc);
	QDF_STATUS (*psoc_send_d0wow_enable_req)(
			struct wlan_objmgr_psoc *psoc);
	QDF_STATUS (*psoc_send_d0wow_disable_req)(
			struct wlan_objmgr_psoc *psoc);
	QDF_STATUS (*psoc_send_idle_roam_suspend_mode)(
			struct wlan_objmgr_psoc *psoc, uint8_t val);

};

#endif /* end  of _WLAN_PMO_OBJ_MGMT_PUBLIC_STRUCT_H_ */
