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
 * DOC: Declare various struct, macros which are common for
 * various pmo related features.
 *
 * Note: This file shall not contain public API's prototype/declartions.
 *
 */

#ifndef _WLAN_PMO_COMMONP_PUBLIC_STRUCT_H_
#define _WLAN_PMO_COMMONP_PUBLIC_STRUCT_H_

#include "wlan_cmn.h"
#include "wlan_objmgr_cmn.h"
#include "wlan_objmgr_global_obj.h"
#include "wmi_unified.h"
#include "qdf_status.h"
#include "qdf_lock.h"
#include "qdf_event.h"
#include "wlan_pmo_hw_filter_public_struct.h"


#define PMO_IPV4_ARP_REPLY_OFFLOAD                  0
#define PMO_IPV6_NEIGHBOR_DISCOVERY_OFFLOAD         1
#define PMO_IPV6_NS_OFFLOAD                         2
#define PMO_OFFLOAD_DISABLE                         0
#define PMO_OFFLOAD_ENABLE                          1

#define PMO_MAC_NS_OFFLOAD_SIZE               1
#define PMO_MAC_NUM_TARGET_IPV6_NS_OFFLOAD_NA 16
#define PMO_IPV6_ADDR_VALID                   1
#define PMO_IPV6_ADDR_UC_TYPE                 0
#define PMO_IPV6_ADDR_AC_TYPE                 1


#define PMO_WOW_REQUIRED_CREDITS 1

#define MAX_MC_IP_ADDR 10
#define IGMP_QUERY_ADDRESS 0x10000e0

/**
 * enum pmo_vdev_param_id: tell vdev param id
 * @pmo_vdev_param_listen_interval: vdev listen interval param id
 * @pmo_vdev_param_dtim_policy: vdev param dtim policy
 * @pmo_vdev_param_forced_dtim_count: vdev param forced dtim count
 * @pmo_vdev_max_param: Max vdev param id
 */
enum pmo_vdev_param_id {
	pmo_vdev_param_listen_interval = 0,
	pmo_vdev_param_dtim_policy,
	pmo_vdev_param_forced_dtim_count,
	pmo_vdev_max_param
};

/**
 * enum pmo_beacon_dtim_policy: tell vdev beacon policy
 * @pmo_ignore_dtim: fwr need to igonre dtime policy
 * @pmo_normal_dtim: fwr need to use normal dtime policy
 * @pmo_stick_dtim: fwr need to use stick dtime policy
 * @auto_dtim: fwr need to auto dtime policy
 */
enum pmo_beacon_dtim_policy {
	pmo_ignore_dtim = 0x01,
	pmo_normal_dtim = 0x02,
	pmo_stick_dtim = 0x03,
	pmo_auto_dtim = 0x04,
};

/**
 * @pmo_sta_ps_param_rx_wake_policy: Controls how frames are retrievd from AP
 *  while STA is sleeping.
 * @pmo_sta_ps_param_tx_wake_threshold: STA will go active after this many TX
 * @pmo_sta_ps_param_pspoll_count:No of PS-Poll to send before STA wakes up
 * @pmo_sta_ps_param_inactivity_time: TX/RX inactivity time in msec before
    going to sleep.
 * @pmo_sta_ps_param_uapsd: Set uapsd configuration.
 * @pmo_sta_ps_param_advanced_power_pspoll_count: No of PS-Poll to send before
    STA wakes up in Advanced Power Save Mode.
 * @pmo_sta_ps_enable_advanced_power:  Enable Advanced Power Save
 * @pmo_sta_ps_param_advanced_power_max_tx_before_wake: Number of TX frames
    before the entering the Active state
 * @pmo_sta_ps_param_ito_repeat_count: Indicates ito repeated count
 */
enum pmo_sta_powersave_param {
	pmo_sta_ps_param_rx_wake_policy = 0,
	pmo_sta_ps_param_tx_wake_threshold = 1,
	pmo_sta_ps_param_pspoll_count = 2,
	pmo_sta_ps_param_inactivity_time = 3,
	pmo_sta_ps_param_uapsd = 4,
	pmo_sta_ps_param_advanced_power_pspoll_count = 5,
	pmo_sta_ps_enable_advanced_power = 6,
	pmo_sta_ps_param_advanced_power_max_tx_before_wake = 7,
	pmo_sta_ps_param_ito_repeat_count = 8,
};

/**
 * enum wow_resume_trigger - resume trigger override setting values
 * @PMO_WOW_RESUME_TRIGGER_DEFAULT: fw to use platform default resume trigger
 * @PMO_WOW_RESUME_TRIGGER_HTC_WAKEUP: force fw to use HTC Wakeup to resume
 * @PMO_WOW_RESUME_TRIGGER_GPIO: force fw to use GPIO to resume
 * @PMO_WOW_RESUME_TRIGGER_COUNT: number of resume trigger options
 */
enum pmo_wow_resume_trigger {
	/* always first */
	PMO_WOW_RESUME_TRIGGER_DEFAULT = 0,
	PMO_WOW_RESUME_TRIGGER_HTC_WAKEUP,
	PMO_WOW_RESUME_TRIGGER_GPIO,
	/* always last */
	PMO_WOW_RESUME_TRIGGER_COUNT
};

/**
 * enum wow_interface_pause - interface pause override setting values
 * @PMO_WOW_INTERFACE_PAUSE_DEFAULT: use platform default iface pause setting
 * @PMO_WOW_INTERFACE_PAUSE_ENABLE: force interface pause setting to enabled
 * @PMO_WOW_INTERFACE_PAUSE_DISABLE: force interface pause setting to disabled
 * @PMO_WOW_INTERFACE_PAUSE_COUNT: number of interface pause options
 */
enum pmo_wow_interface_pause {
	/* always first */
	PMO_WOW_INTERFACE_PAUSE_DEFAULT = 0,
	PMO_WOW_INTERFACE_PAUSE_ENABLE,
	PMO_WOW_INTERFACE_PAUSE_DISABLE,
	/* always last */
	PMO_WOW_INTERFACE_PAUSE_COUNT
};

/**
 * enum wow_enable_type - used to enable/disable WoW.
 * @PMO_WOW_DISABLE_BOTH: Disable both magic pattern match and pattern
 *  byte match.
 * @PMO_WOW_ENABLE_MAGIC_PATTERN: Enable magic pattern match on all interfaces.
 * @PMO_WOW_ENABLE_PATTERN_BYTE: Enable pattern byte match on all interfaces.
 * @PMO_WOW_ENABLE_BOTH: Enable both magic patter and pattern byte match on
 *  all interfaces.
 */
enum pmo_wow_enable_type {
	PMO_WOW_DISABLE_BOTH = 0,
	PMO_WOW_ENABLE_MAGIC_PATTERN,
	PMO_WOW_ENABLE_PATTERN_BYTE,
	PMO_WOW_ENABLE_BOTH
};

/**
 * enum powersave_mode - powersave_mode
 * @PMO_PS_ADVANCED_POWER_SAVE_DISABLE: Disable advanced power save mode
 * @PMO_PS_ADVANCED_POWER_SAVE_ENABLE: Enable power save mode
 */
enum powersave_mode {
	PMO_PS_ADVANCED_POWER_SAVE_DISABLE = 0,
	PMO_PS_ADVANCED_POWER_SAVE_ENABLE = 1
};

#define PMO_TARGET_SUSPEND_TIMEOUT   (4000)
#define PMO_WAKE_LOCK_TIMEOUT        1000
#define PMO_RESUME_TIMEOUT           (4000)

/**
 * struct wow_enable_params - A collection of wow enable override parameters
 * @is_unit_test: true to notify fw this is a unit-test suspend
 * @interface_pause: used to override the interface pause indication sent to fw
 * @resume_trigger: used to force fw to use a particular resume method
 */
struct pmo_wow_enable_params {
	bool is_unit_test;
	enum pmo_wow_interface_pause interface_pause;
	enum pmo_wow_resume_trigger resume_trigger;
};

/**
 * typedef for psoc suspend handler
 */
typedef QDF_STATUS(*pmo_psoc_suspend_handler)
	(struct wlan_objmgr_psoc *psoc, void *arg);
/**
 * typedef for psoc resume handler
 */
typedef QDF_STATUS(*pmo_psoc_resume_handler)
	(struct wlan_objmgr_psoc *psoc, void *arg);

/**
 * enum pmo_offload_trigger: trigger information
 * @pmo_apps_suspend: trigger is apps suspend
 * @pmo_apps_resume: trigger is apps resume
 * @pmo_runtime_suspend: trigger is runtime suspend
 * @pmo_runtime_resume: trigger is runtime resume
 * @pmo_ipv4_change_notify: trigger is ipv4 change handler
 * @pmo_ipv6_change_notify: trigger is ipv6 change handler
 * @pmo_ns_offload_dynamic_update: enable/disable ns offload on the fly
 * @pmo_peer_disconnect: trigger is peer disconnect
 * @pmo_mcbc_setting_dynamic_update: mcbc value update on the fly
 *
 * @pmo_offload_trigger_max: Max trigger value
 */
enum pmo_offload_trigger {
	pmo_apps_suspend = 0,
	pmo_apps_resume,
	pmo_runtime_suspend,
	pmo_runtime_resume,
	pmo_ipv4_change_notify,
	pmo_ipv6_change_notify,
	pmo_mc_list_change_notify,
	pmo_ns_offload_dynamic_update,
	pmo_peer_disconnect,
	pmo_mcbc_setting_dynamic_update,

	pmo_offload_trigger_max,
};

/**
 * enum pmo_auto_pwr_detect_failure_mode_t - auto detect failure modes
 * @PMO_FW_TO_CRASH_ON_PWR_FAILURE: Don't register wow wakeup event and FW
 * crashes on power failure
 * @PMO_FW_TO_SEND_WOW_IND_ON_PWR_FAILURE: Register wow wakeup event and FW
 * sends failure event to host on power failure
 * @PMO_FW_TO_REJUVENATE_ON_PWR_FAILURE: Don't register wow wakeup event and
 * FW silently rejuvenate on power failure
 * @PMO_AUTO_PWR_FAILURE_DETECT_DISABLE: Don't register wow wakeup event and the
 * auto power failure detect feature is disabled in FW.
 */
enum pmo_auto_pwr_detect_failure_mode {
	PMO_FW_TO_CRASH_ON_PWR_FAILURE,
	PMO_FW_TO_SEND_WOW_IND_ON_PWR_FAILURE,
	PMO_FW_TO_REJUVENATE_ON_PWR_FAILURE,
	PMO_AUTO_PWR_FAILURE_DETECT_DISABLE
};

/**
 * enum active_apf_mode - the modes active APF can operate in
 * @ACTIVE_APF_DISABLED: APF is disabled in active mode
 * @ACTIVE_APF_ENABLED: APF is enabled for all packets
 * @ACTIVE_APF_ADAPTIVE: APF is enabled for packets up to some threshold
 * @ACTIVE_APF_MODE_COUNT: The number of active APF modes
 */
enum active_apf_mode {
	ACTIVE_APF_DISABLED = 0,
	ACTIVE_APF_ENABLED,
	ACTIVE_APF_ADAPTIVE,
	ACTIVE_APF_MODE_COUNT
};

/**
 * struct pmo_psoc_cfg - user configuration required for pmo
 * @ptrn_match_enable_all_vdev: true when pattern match is enable for all vdev
 * @apf_enable: true if psoc supports apf else false
 * @arp_offload_enable: true if arp offload is supported for psoc else false
 * @hw_filter_mode_bitmap: which mode the hardware filter should use during DTIM
 * @ns_offload_enable_static: true if psoc supports ns offload in ini else false
 * @ns_offload_enable_dynamic: to enable / disable the ns offload using
 *    ioctl or vendor command.
 * @packet_filter_enabled: true if feature is enabled by configuration
 * @ssdp:  true if psoc supports if ssdp configuration in wow mode
 * @enable_mc_list: true if psoc supports mc addr list else false
 * @active_mode_offload: true if psoc supports active mode offload else false
 * @ap_arpns_support: true if psoc supports arp ns for ap mode
 * @d0_wow_supported: true if psoc supports D0 wow command
 * @ra_ratelimit_enable: true when ra filtering ins eanbled else false
 * @ra_ratelimit_interval: ra packets interval
 * @magic_ptrn_enable: true when magic pattern is enabled else false
 * @deauth_enable: true when wake up on deauth is enabled else false
 * @disassoc_enable:  true when wake up on disassoc is enabled else false
 * @lpass_enable: true when lpass is enabled else false
 * @max_ps:poll: max power save poll
 * @sta_dynamic_dtim: station dynamic DTIM value
 * @sta_mod_dtim: station modulated DTIM value
 * @sta_max_li_mod_dtim: station max listen interval DTIM value
 * @sta_forced_dtim: station forced DTIM value
 * @wow_enable: enable wow with majic pattern match or pattern byte match
 * @power_save_mode: power save mode for psoc
 * @runtime_pm_delay: set runtime pm's inactivity timer
 * @extwow_goto_suspend: true when extended WoW enabled else false
 * @extwow_app1_wakeup_pin_num: set wakeup1 PIN number
 * @extwow_app2_wakeup_pin_num: set wakeup2 PIN number
 * @extwow_app2_init_ping_interval: set keep alive init ping interval
 * @extwow_app2_min_ping_interval: set keep alive minimum ping interval
 * @extwow_app2_max_ping_interval: set keep alive maximum ping interval
 * @extwow_app2_inc_ping_interval: set keep alive increment ping interval
 * @extwow_app2_tcp_src_port: set TCP source port
 * @extwow_app2_tcp_dst_port: set TCP dest port
 * @extwow_app2_tcp_tx_timeout: set TCP TX timeout
 * @extwow_app2_tcp_rx_timeout: set TCP RX timeout
 * @auto_power_save_fail_mode: auto detect power save failure
 * @is_wow_pulse_supported: true when wow pulse feature is enabled else false
 * @wow_pulse_pin: GPIO pin of wow pulse feature
 * @wow_pulse_interval_high: The interval of high level in the pulse
 * @wow_pulse_interval_low: The interval of low level in the pulse
 * @wow_pulse_repeat_count: Pulse repeat count
 * @wow_pulse_init_state: Pulse init level
 * @packet_filters_bitmap: Packet filter bitmap configuration
 * @wow_data_inactivity_timeout: power save wow data inactivity timeout
 * @ps_data_inactivity_timeout: Power save data inactivity timeout for non
 *  wow mode
 * @active_uc_apf_mode: Setting that determines how APF is applied in active
 *	mode for uc packets
 * @active_mc_bc_apf_mode: Setting that determines how APF is applied in
 *	active mode for MC/BC packets
 * @ito_repeat_count: Indicates ito repeated count
 * @is_mod_dtim_on_sys_suspend_enabled: true when mod dtim is enabled for
 * system suspend wow else false
 * @igmp_version_support: igmp version support
 * @igmp_offload_enable: enable/disable igmp offload feature to fw
 */
struct pmo_psoc_cfg {
	bool ptrn_match_enable_all_vdev;
	bool apf_enable;
	bool arp_offload_enable;
	enum pmo_hw_filter_mode hw_filter_mode_bitmap;
	bool ns_offload_enable_static;
	bool ns_offload_enable_dynamic;
	bool packet_filter_enabled;
	bool ssdp;
	bool enable_mc_list;
	bool active_mode_offload;
	bool ap_arpns_support;
	bool d0_wow_supported;
	bool ra_ratelimit_enable;
#if FEATURE_WLAN_RA_FILTERING
	uint16_t ra_ratelimit_interval;
#endif
	bool magic_ptrn_enable;
	bool deauth_enable;
	bool disassoc_enable;
	bool lpass_enable;
	uint8_t max_ps_poll;
	uint8_t sta_dynamic_dtim;
	uint8_t sta_mod_dtim;
	uint8_t sta_max_li_mod_dtim;
	bool sta_forced_dtim;
	enum pmo_wow_enable_type wow_enable;
	enum powersave_mode power_save_mode;
	enum powersave_mode default_power_save_mode;
#ifdef FEATURE_RUNTIME_PM
	uint32_t runtime_pm_delay;
#endif
#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
	bool extwow_goto_suspend;
	uint8_t extwow_app1_wakeup_pin_num;
	uint8_t extwow_app2_wakeup_pin_num;
	uint32_t extwow_app2_init_ping_interval;
	uint32_t extwow_app2_min_ping_interval;
	uint32_t extwow_app2_max_ping_interval;
	uint32_t extwow_app2_inc_ping_interval;
	uint16_t extwow_app2_tcp_src_port;
	uint16_t extwow_app2_tcp_dst_port;
	uint32_t extwow_app2_tcp_tx_timeout;
	uint32_t extwow_app2_tcp_rx_timeout;
#endif
	enum pmo_auto_pwr_detect_failure_mode auto_power_save_fail_mode;
#ifdef WLAN_FEATURE_WOW_PULSE
	bool is_wow_pulse_supported;
	uint8_t wow_pulse_pin;
	uint16_t wow_pulse_interval_high;
	uint16_t wow_pulse_interval_low;
	uint32_t wow_pulse_repeat_count;
	uint32_t wow_pulse_init_state;
#endif
#ifdef WLAN_FEATURE_PACKET_FILTERING
	uint8_t packet_filters_bitmap;
#endif
	bool enable_sap_suspend;
	uint8_t wow_data_inactivity_timeout;
	uint8_t ps_data_inactivity_timeout;
	enum active_apf_mode active_uc_apf_mode;
	enum active_apf_mode active_mc_bc_apf_mode;
	uint8_t ito_repeat_count;
	bool is_mod_dtim_on_sys_suspend_enabled;
	bool is_bus_suspend_enabled_in_sap_mode;
	bool is_bus_suspend_enabled_in_go_mode;
#ifdef WLAN_FEATURE_IGMP_OFFLOAD
	uint32_t igmp_version_support;
	bool igmp_offload_enable;
#endif
};

/**
 * pmo_device_caps - device capability flags (true if feature is supported)
 * @apf: Android Packet Filter (aka BPF)
 * @arp_ns_offload: APR/NS offload
 * @packet_filter: Legacy "Packet Filter"
 * @unified_wow: Firmware supports "interface pause" flag in WoW command.
 *	This allows both D0-WoW (bus up) and Non-D0-WoW (bus down) to use one
 *	unified command
 * @li_offload: Firmware has listen interval offload support
 */
struct pmo_device_caps {
	bool apf;
	bool arp_ns_offload;
	bool packet_filter;
	bool unified_wow;
	bool li_offload;
};

/**
 * pmo_igmp_offload_req - structure to hold igmp param
 *
 * @vdev_id: vdev id
 * @enable: enable/disable
 * @version_support: version support
 * @num_grp_ip_address: num grp ip addr
 * @grp_ip_address: array of grp_ip_address
 *
 **/
struct pmo_igmp_offload_req {
	uint32_t vdev_id;
	bool enable;
	uint32_t version_support;
	uint32_t num_grp_ip_address;
	uint32_t grp_ip_address[MAX_MC_IP_ADDR];
};
#endif /* end  of _WLAN_PMO_COMMONP_STRUCT_H_ */
