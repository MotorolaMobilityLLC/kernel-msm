/*
 * Copyright (c) 2012, 2014-2021 The Linux Foundation. All rights reserved.
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

#ifndef __WLAN_HDD_POWER_H
#define __WLAN_HDD_POWER_H

/**
 * DOC: wlan_hdd_power.h
 *
 * HDD Power Management API
 */

#include "wlan_hdd_main.h"
#include <linux/pm_qos.h>
#include <linux/pm_runtime.h>

#define HDD_WAKELOCK_TIMEOUT_CONNECT 1000
#define HDD_WAKELOCK_TIMEOUT_RESUME 1000
#define DISABLE_KRAIT_IDLE_PS_VAL      1

/*
 * HDD_WAKELOCK_CONNECT_COMPLETE = CSR_JOIN_FAILURE_TIMEOUT_DEFAULT (3000) +
 *                      WNI_CFG_AUTHENTICATE_FAILURE_TIMEOUT_STADEF (1000) +
 *                      WNI_CFG_ASSOCIATION_FAILURE_TIMEOUT_STADEF  (2000)
 */
#define HDD_WAKELOCK_CONNECT_COMPLETE 6000

#ifdef WLAN_FEATURE_PACKET_FILTERING

#define HDD_MAX_CMP_PER_PACKET_FILTER	5

/**
 * enum pkt_filter_protocol_layer - packet filter protocol layer
 * @HDD_FILTER_PROTO_TYPE_INVALID: Invalid initial value
 * @HDD_FILTER_PROTO_TYPE_MAC: MAC protocol
 * @HDD_FILTER_PROTO_TYPE_ARP: ARP protocol
 * @HDD_FILTER_PROTO_TYPE_IPV4: IP V4 protocol
 * @HDD_FILTER_PROTO_TYPE_IPV6: IP V6 protocol
 * @HDD_FILTER_PROTO_TYPE_UDP: UDP protocol
 * @HDD_FILTER_PROTO_TYPE_INVALID: Max place holder value
 */
enum pkt_filter_protocol_layer {
	HDD_FILTER_PROTO_TYPE_INVALID = 0,
	HDD_FILTER_PROTO_TYPE_MAC = 1,
	HDD_FILTER_PROTO_TYPE_ARP = 2,
	HDD_FILTER_PROTO_TYPE_IPV4 = 3,
	HDD_FILTER_PROTO_TYPE_IPV6 = 4,
	HDD_FILTER_PROTO_TYPE_UDP = 5,
	HDD_FILTER_PROTO_TYPE_MAX
};

/**
 * enum pkt_filter_action - packet filter action
 * @HDD_RCV_FILTER_INVALID: Invalid initial value
 * @HDD_RCV_FILTER_SET: Packet filter set
 * @HDD_RCV_FILTER_CLEAR: Packet filter clear
 * @HDD_RCV_FILTER_MAX: Max place holder value
 */
enum pkt_filter_action {
	HDD_RCV_FILTER_INVALID = 0,
	HDD_RCV_FILTER_SET = 1,
	HDD_RCV_FILTER_CLEAR = 2,
	HDD_RCV_FILTER_MAX
};

/**
 * enum pkt_filter_compare_flag - packet filter compare flag
 * @HDD_FILTER_CMP_TYPE_INVALID: Invalid initial value
 * @HDD_FILTER_CMP_TYPE_EQUAL: Compare if filter is equal
 * @HDD_FILTER_CMP_TYPE_MASK_EQUAL: Compare if filter mask is equal
 * @HDD_FILTER_CMP_TYPE_NOT_EQUAL: Compare if filter is not equal
 * @HDD_FILTER_CMP_TYPE_MASK_NOT_EQUAL: Compare if filter mask is not equal
 * @HDD_FILTER_CMP_TYPE_MAX: Max place holder value
 */
enum pkt_filter_compare_flag {
	HDD_FILTER_CMP_TYPE_INVALID = 0,
	HDD_FILTER_CMP_TYPE_EQUAL = 1,
	HDD_FILTER_CMP_TYPE_MASK_EQUAL = 2,
	HDD_FILTER_CMP_TYPE_NOT_EQUAL = 3,
	HDD_FILTER_CMP_TYPE_MASK_NOT_EQUAL = 4,
	HDD_FILTER_CMP_TYPE_MAX
};

/**
 * struct pkt_filter_param_cfg - packet filter parameter config
 * @protocol_layer: Protocol layer
 * @compare_flag: Compare flag
 * @data_fffset: Data offset
 * @data_length: Data length
 * @compare_data: Compare data
 * @data_mask: Data mask
 */
struct pkt_filter_param_cfg {
	uint8_t protocol_layer;
	uint8_t compare_flag;
	uint8_t data_offset;
	uint8_t data_length;
	uint8_t compare_data[SIR_MAX_FILTER_TEST_DATA_LEN];
	uint8_t data_mask[SIR_MAX_FILTER_TEST_DATA_LEN];
};

/**
 * struct pkt_filter_cfg - packet filter config received from user space
 * @filter_action: Filter action
 * @filter_id: Filter id
 * @num_params: Number of parameters
 * @params_data: Packet filter parameters detail
 */
struct pkt_filter_cfg {
	uint8_t filter_action;
	uint8_t filter_id;
	uint8_t num_params;
	struct pkt_filter_param_cfg params_data[HDD_MAX_CMP_PER_PACKET_FILTER];
};

#endif

#ifdef FEATURE_ANI_LEVEL_REQUEST
/**
 * ani_priv - structure to store the priv data for get ani request
 * @num_freq: number of freq received from the FW
 * @ani: data received from the FW
 */
struct ani_priv {
	uint32_t num_freq;
	struct wmi_host_ani_level_event *ani;
};
#endif

/**
 * enum suspend_resume_state - Suspend resume state
 * @HDD_WLAN_EARLY_SUSPEND: Early suspend state.
 * @HDD_WLAN_SUSPEND: Suspend state.
 * @HDD_WLAN_EARLY_RESUME: Early resume state.
 * @HDD_WLAN_RESUME: Resume state.
 *
 * Suspend state to indicate in diag event of suspend resume.
 */
enum suspend_resume_state {
	 HDD_WLAN_EARLY_SUSPEND,
	 HDD_WLAN_SUSPEND,
	 HDD_WLAN_EARLY_RESUME,
	 HDD_WLAN_RESUME
};

/**
 * hdd_svc_fw_shutdown_ind() - API to send FW SHUTDOWN IND to Userspace
 * @dev: Device Pointer
 *
 * Return: None
 */
void hdd_svc_fw_shutdown_ind(struct device *dev);

/**
 * hdd_wlan_shutdown() - HDD SSR shutdown function
 *
 * This function is called by the HIF to shutdown the driver during SSR.
 *
 * Return: QDF_STATUS_SUCCESS if the driver was shut down,
 *	or an error status otherwise
 */
QDF_STATUS hdd_wlan_shutdown(void);

/**
 * hdd_wlan_re_init() - HDD SSR re-init function
 *
 * This function is called by the HIF to re-initialize the driver after SSR.
 *
 * Return: QDF_STATUS_SUCCESS if the driver was re-initialized,
 *	or an error status otherwise
 */
QDF_STATUS hdd_wlan_re_init(void);

/**
 * hdd_handle_cached_commands() - Handle north bound commands during SSR
 *
 * This api will be invoked afte SSR re-initialization to execute the north
 * bound commands received during SSR.
 *
 * Return: None
 */
void hdd_handle_cached_commands(void);

/**
 * hdd_enable_arp_offload() - API to enable ARP offload
 * @adapter: Adapter context for which ARP offload is to be configured
 * @trigger: trigger reason for request
 *
 * Return: None
 */
void hdd_enable_arp_offload(struct hdd_adapter *adapter,
			    enum pmo_offload_trigger trigger);

/**
 * hdd_disable_arp_offload() - API to disable ARP offload
 * @adapter: Adapter context for which ARP offload is to be configured
 * @trigger: trigger reason for request
 *
 * Return: None
 */
void hdd_disable_arp_offload(struct hdd_adapter *adapter,
			     enum pmo_offload_trigger trigger);

/**
 * hdd_enable_host_offloads() - Central API to enable the supported offloads
 * @adapter:   pointer to the adapter
 * @trigger: trigger reason for request
 *
 * Central function to enable the supported offloads
 *
 * Return: nothing
 */
void hdd_enable_host_offloads(struct hdd_adapter *adapter,
			      enum pmo_offload_trigger trigger);

/**
 * hdd_disable_host_offloads() - Central API to disable the supported offloads
 * @adapter:   pointer to the adapter
 * @trigger: trigger reason for request
 *
 * Central function to disable the supported offloads
 *
 * Return: nothing
 */
void hdd_disable_host_offloads(struct hdd_adapter *adapter,
			       enum pmo_offload_trigger trigger);

/**
 * hdd_enable_mc_addr_filtering() - enable MC address list in FW
 * @adapter: adapter whose MC list is being set
 * @trigger: trigger reason for request
 *
 * Return: nothing
 */
void hdd_enable_mc_addr_filtering(struct hdd_adapter *adapter,
				  enum pmo_offload_trigger trigger);

/**
 * hdd_disable_mc_addr_filtering() - disable MC address list in FW
 * @adapter: adapter whose MC list is being set
 * @trigger: trigger reason for request
 *
 * Return: nothing
 */
void hdd_disable_mc_addr_filtering(struct hdd_adapter *adapter,
				   enum pmo_offload_trigger trigger);

/**
 * hdd_cache_mc_addr_list() - API to cache MC address list
 * @mc_list_config: set of mc address list configurations
 *
 * Return: 0 on success else error code
 */
int hdd_cache_mc_addr_list(struct pmo_mc_addr_list_params *mc_list_config);

/**
 * hdd_disable_and_flush_mc_addr_list() - API to Disable & Flush cached MC list
 * @adapter: adapter whose MC list is being set
 * @trigger: trigger reason for request
 *
 * Return: nothing
 */
void hdd_disable_and_flush_mc_addr_list(struct hdd_adapter *adapter,
					enum pmo_offload_trigger trigger);

/**
 * wlan_hdd_cfg80211_update_replay_counter_cb() - replay counter callback
 * @cb_ctx: Callback context as void* as PMO do not about HDD adapter type
 * @gtk_rsp_param: Pointer to gtk offload response parameter
 *
 * Callback routine called upon receiving of gtk offload rsp from fwr
 *
 * Return: none
 */
void wlan_hdd_cfg80211_update_replay_counter_cb(
	void *cb_ctx,
	struct pmo_gtk_rsp_params *gtk_rsp_param);

/**
 * wlan_hdd_cfg80211_suspend_wlan() - cfg80211 suspend callback
 * @wiphy: Pointer to wiphy
 * @wow: Pointer to wow
 *
 * This API is called when cfg80211 driver suspends
 *
 * Return: integer status
 */
int wlan_hdd_cfg80211_suspend_wlan(struct wiphy *wiphy,
				   struct cfg80211_wowlan *wow);

/**
 * wlan_hdd_cfg80211_resume_wlan() - cfg80211 resume callback
 * @wiphy: Pointer to wiphy
 *
 * This API is called when cfg80211 driver resumes driver updates
 * latest sched_scan scan result(if any) to cfg80211 database
 *
 * Return: integer status
 */
int wlan_hdd_cfg80211_resume_wlan(struct wiphy *wiphy);

/**
 * hdd_ipv4_notifier_work_queue() - IP V4 change notifier work handler
 * @work: Pointer to work context
 *
 * Return: none
 */
void hdd_ipv4_notifier_work_queue(struct work_struct *work);

#ifdef WLAN_NS_OFFLOAD
/**
 * hdd_enable_ns_offload() - enable NS offload
 * @adapter:   pointer to the adapter
 *
 * Return: nothing
 */
void hdd_enable_ns_offload(struct hdd_adapter *adapter,
			   enum pmo_offload_trigger trigger);

/**
 * hdd_disable_ns_offload() - disable NS offload
 * @adapter:   pointer to the adapter
 *
 * Return: nothing
 */
void hdd_disable_ns_offload(struct hdd_adapter *adapter,
	enum pmo_offload_trigger trigger);
#else /* WLAN_NS_OFFLOAD */
static inline
void hdd_enable_ns_offload(struct hdd_adapter *adapter,
			   enum pmo_offload_trigger trigger)
{
}

static inline
void hdd_disable_ns_offload(struct hdd_adapter *adapter,
			    enum pmo_offload_trigger trigger)
{
}
#endif /* WLAN_NS_OFFLOAD */

/**
 * hdd_ipv6_notifier_work_queue() - IP V6 change notifier work handler
 * @work: Pointer to work context
 *
 * Return: none
 */
void hdd_ipv6_notifier_work_queue(struct work_struct *work);

/**
 * wlan_hdd_cfg80211_get_txpower() - cfg80211 get power handler function
 * @wiphy: Pointer to wiphy structure.
 * @wdev: Pointer to wireless_dev structure.
 * @dbm: dbm
 *
 * This is the cfg80211 get txpower handler function which invokes
 * the internal function @__wlan_hdd_cfg80211_get_txpower with
 * SSR protection.
 *
 * Return: 0 for success, error number on failure.
 */
int wlan_hdd_cfg80211_get_txpower(struct wiphy *wiphy,
				  struct wireless_dev *wdev,
				  int *dbm);

/**
 * wlan_hdd_cfg80211_set_txpower() - set TX power
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to network device
 * @type: TX power setting type
 * @dbm: TX power in dbm
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_hdd_cfg80211_set_txpower(struct wiphy *wiphy,
				  struct wireless_dev *wdev,
				  enum nl80211_tx_power_setting type,
				  int dbm);

/**
 * wlan_hdd_cfg80211_set_power_mgmt() - set cfg80211 power management config
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @allow_power_save: is wlan allowed to go into power save mode
 * @timeout: Timeout value
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_hdd_cfg80211_set_power_mgmt(struct wiphy *wiphy,
				     struct net_device *dev,
				     bool allow_power_save,
				     int timeout);

/**
 * wlan_hdd_ipv4_changed() - IPv4 change notifier callback
 * @nb: pointer to notifier block
 * @data: data
 * @arg: arg
 *
 * This is the IPv4 notifier callback function gets invoked
 * if any change in IP and then invoke the function @__wlan_hdd_ipv4_changed
 * to reconfigure the offload parameters.
 *
 * Return: 0 on success, error number otherwise.
 */
int wlan_hdd_ipv4_changed(struct notifier_block *nb,
			  unsigned long data, void *arg);

#ifdef FEATURE_RUNTIME_PM
/**
 * wlan_hdd_pm_qos_notify() - PM QOS notifier call back function
 * @nb: Pointer to notifier block kernel structure
 * @curr_val: PM QOS current value
 * @context: call back context
 *
 * This callback function for PM QOS change notification is used to setup
 * dynamic runtime PM.
 *
 * Return: NOTIFY_DONE for success
 */
int wlan_hdd_pm_qos_notify(struct notifier_block *nb, unsigned long curr_val,
			   void *context);
#endif
/**
 * wlan_hdd_ipv6_changed() - IPv6 change notifier callback
 * @nb: pointer to notifier block
 * @data: data
 * @arg: arg
 *
 * This is the IPv6 notifier callback function gets invoked
 * if any change in IP and then invoke the function @__wlan_hdd_ipv6_changed
 * to reconfigure the offload parameters.
 *
 * Return: 0 on success, error number otherwise.
 */
int wlan_hdd_ipv6_changed(struct notifier_block *nb,
			  unsigned long data, void *arg);

/**
 * hdd_set_power_config() - set power config to firmware
 * @hddctx: HDD context
 * @adapter: HDD adapter
 * @power: new power config value
 *
 * Return: 0 on success; Errno on failure
 */
int hdd_set_power_config(struct hdd_context *hddctx,
			 struct hdd_adapter *adapter, uint8_t power);

#ifdef FEATURE_WLAN_DIAG_SUPPORT
/**
 * hdd_wlan_suspend_resume_event()- send suspend/resume state
 * @state: suspend/resume state
 *
 * This Function sends suspend resume state diag event
 *
 * Return: void.
 */
void hdd_wlan_suspend_resume_event(uint8_t state);

#else
static inline
void hdd_wlan_suspend_resume_event(uint8_t state) {}
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

/**
 * wlan_hdd_set_powersave() - Set powersave mode
 * @adapter: adapter upon which the request was received
 * @allow_power_save: is wlan allowed to go into power save mode
 * @timeout: timeout period in ms
 *
 * Return: 0 on success, non-zero on any error
 */
int wlan_hdd_set_powersave(struct hdd_adapter *adapter,
			   bool allow_power_save, uint32_t timeout);

/**
 * wlan_hdd_inc_suspend_stats() - Prints, then increments, then prints suspend
 *	failed statistics.
 * @hdd_ctx:	The HDD context to operate on
 * @reason:	The suspend failed reason to increment
 *
 * This function prints all of the suspend failed statistics, increments the
 * specified suspend fail reason statistic, and prints the them all again. This
 * is for easily keeping track of the most common reasons suspend fails.
 *
 * Return: none
 */
void wlan_hdd_inc_suspend_stats(struct hdd_context *hdd_ctx,
				enum suspend_fail_reason reason);

/*
 * Unit-test suspend/resume is a testing feature that allows putting firmware
 * into WoW suspend irrespective of Apps suspend status. It emulates the chain
 * of events that occur durring normal system-level suspend/resume, such as
 * initiating all of the suspend/resume stages in the correct order, and
 * enabling/disabling appropriate copy engine irqs.
 */
#ifdef WLAN_SUSPEND_RESUME_TEST
/**
 * wlan_hdd_unit_test_bus_suspend() - suspend the wlan bus
 * @wow_params: collection of wow enable override parameters
 *
 * This function does the same as wlan_hdd_bus_suspend, but additionally passes
 * the appropriate flags to FW, indicating this is a unit-test suspend and it
 * should use an HTC wakeup method to resume.
 *
 * Return: 0 for success or error code
 */
int wlan_hdd_unit_test_bus_suspend(struct wow_enable_params wow_params);

/**
 * hdd_wlan_fake_apps_resume() - Resume from unit-test triggered suspend
 * @wiphy: the kernel wiphy struct for the device being resumed
 * @dev: the kernel net_device struct for the device being resumed
 *
 * Return: Zero on success, calls QDF_BUG() on failure
 */
int hdd_wlan_fake_apps_resume(struct wiphy *wiphy, struct net_device *dev);

/**
 * hdd_wlan_fake_apps_suspend() - Initiate a unit-test triggered suspend
 * @wiphy: the kernel wiphy struct for the device being suspended
 * @dev: the kernel net_device struct for the device being suspended
 * @pause_setting: interface pause override setting
 * @resume_setting: resume trigger override setting
 *
 * Return: Zero on success, suspend related non-zero error code on failure
 */
int hdd_wlan_fake_apps_suspend(struct wiphy *wiphy, struct net_device *dev,
			       enum wow_interface_pause pause_setting,
			       enum wow_resume_trigger resume_setting);
#else
static inline int
hdd_wlan_fake_apps_resume(struct wiphy *wiphy, struct net_device *dev)
{
	return 0;
}

static inline int
hdd_wlan_fake_apps_suspend(struct wiphy *wiphy, struct net_device *dev,
			   enum wow_interface_pause pause_setting,
			   enum wow_resume_trigger resume_setting)
{
	return 0;
}
#endif /* WLAN_SUSPEND_RESUME_TEST */

#ifdef QCA_CONFIG_SMP
/**
 * wlan_hdd_rx_thread_resume() - Resume RX thread
 * @hdd_ctx: HDD context
 *
 * Check if RX thread suspended, and resume if yes.
 *
 * Return: None
 */
void wlan_hdd_rx_thread_resume(struct hdd_context *hdd_ctx);

/**
 * wlan_hdd_rx_thread_suspend() - Suspend RX thread
 * @hdd_ctx: HDD context
 *
 * To suspend RX thread
 *
 * Return: 0 for success
 */
int wlan_hdd_rx_thread_suspend(struct hdd_context *hdd_ctx);

#else
static inline void wlan_hdd_rx_thread_resume(struct hdd_context *hdd_ctx) {}
static inline int wlan_hdd_rx_thread_suspend(struct hdd_context *hdd_ctx)
{
	return 0;
}
#endif

#ifdef FEATURE_ANI_LEVEL_REQUEST
/**
 * wlan_hdd_get_ani_level() - Wrapper to call API to fetch ani level
 * @adapter: pointer to HDD adapter
 * @ani: pointer to structure storing ani level for channels
 * @parsed_freqs: parsed freqs from the get ani command
 * @num_freqs: number of parsed channels
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_hdd_get_ani_level(struct hdd_adapter *adapter,
				  struct wmi_host_ani_level_event *ani,
				  uint32_t *parsed_freqs,
				  uint8_t num_freqs);
#endif /* FEATURE_ANI_LEVEL_REQUEST */
#endif /* __WLAN_HDD_POWER_H */
