/*
 * Copyright (c) 2012-2019, 2021 The Linux Foundation. All rights reserved.
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

#ifndef __P2P_H
#define __P2P_H

/**
 * DOC: wlan_hdd_p2p.h
 *
 * Linux HDD P2P include file
 */

#define WLAN_HDD_GET_TYPE_FRM_FC(__fc__)         (((__fc__) & 0x0F) >> 2)
#define WLAN_HDD_GET_SUBTYPE_FRM_FC(__fc__)      (((__fc__) & 0xF0) >> 4)
#define WLAN_HDD_80211_FRM_DA_OFFSET             4

#define P2P_POWER_SAVE_TYPE_OPPORTUNISTIC        (1 << 0)
#define P2P_POWER_SAVE_TYPE_PERIODIC_NOA         (1 << 1)
#define P2P_POWER_SAVE_TYPE_SINGLE_NOA           (1 << 2)

struct p2p_app_set_ps {
	uint8_t opp_ps;
	uint32_t ct_window;
	uint8_t count;
	uint32_t duration;
	uint32_t interval;
	uint32_t single_noa_duration;
	uint8_t ps_selection;
};

int wlan_hdd_cfg80211_remain_on_channel(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					struct ieee80211_channel *chan,
					unsigned int duration, u64 *cookie);

int wlan_hdd_cfg80211_cancel_remain_on_channel(struct wiphy *wiphy,
					       struct wireless_dev *wdev,
					       u64 cookie);

int wlan_hdd_cfg80211_mgmt_tx_cancel_wait(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  u64 cookie);

int hdd_set_p2p_ps(struct net_device *dev, void *msgData);
int hdd_set_p2p_opps(struct net_device *dev, uint8_t *command);
int hdd_set_p2p_noa(struct net_device *dev, uint8_t *command);

/**
 * hdd_indicate_mgmt_frame_to_user- send mgmt frame to user
 * @adapter: adapter pointer
 * @frm_len: frame length
 * @pb_frames: frame bytes
 * @frame_type: frame type
 * @rx_freq: frequency on which frame was received
 * @rx_rssi: rssi
 * @rx_flags: rx flags of the frame
 */
void hdd_indicate_mgmt_frame_to_user(struct hdd_adapter *adapter,
				     uint32_t frm_len, uint8_t *pb_frames,
				     uint8_t frame_type, uint32_t rx_freq,
				     int8_t rx_rssi,
				     enum rxmgmt_flags rx_flags);

int wlan_hdd_check_remain_on_channel(struct hdd_adapter *adapter);
void wlan_hdd_cancel_existing_remain_on_channel(struct hdd_adapter *adapter);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
int wlan_hdd_mgmt_tx(struct wiphy *wiphy, struct wireless_dev *wdev,
		     struct cfg80211_mgmt_tx_params *params, u64 *cookie);
#else
int wlan_hdd_mgmt_tx(struct wiphy *wiphy, struct wireless_dev *wdev,
		     struct ieee80211_channel *chan, bool offchan,
		     unsigned int wait,
		     const u8 *buf, size_t len, bool no_cck,
		     bool dont_wait_for_ack, u64 *cookie);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
struct wireless_dev *wlan_hdd_add_virtual_intf(struct wiphy *wiphy,
					       const char *name,
					       unsigned char name_assign_type,
					       enum nl80211_iftype type,
					       struct vif_params *params);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)) || defined(WITH_BACKPORTS)
struct wireless_dev *wlan_hdd_add_virtual_intf(struct wiphy *wiphy,
					       const char *name,
					       unsigned char name_assign_type,
					       enum nl80211_iftype type,
					       u32 *flags,
					       struct vif_params *params);
#else
struct wireless_dev *wlan_hdd_add_virtual_intf(struct wiphy *wiphy,
					       const char *name,
					       enum nl80211_iftype type,
					       u32 *flags,
					       struct vif_params *params);

#endif

/**
 * hdd_clean_up_interface() - clean up hdd interface
 * @hdd_ctx: pointer to hdd context
 * @adapter: pointer to adapter
 *
 * This function clean up hdd interface.
 *
 * Return: None
 */
void hdd_clean_up_interface(struct hdd_context *hdd_ctx,
			    struct hdd_adapter *adapter);
int wlan_hdd_del_virtual_intf(struct wiphy *wiphy, struct wireless_dev *wdev);
int __wlan_hdd_del_virtual_intf(struct wiphy *wiphy, struct wireless_dev *wdev);


void wlan_hdd_cleanup_remain_on_channel_ctx(struct hdd_adapter *adapter);

/**
 * wlan_hdd_set_power_save() - hdd set power save
 * @adapter:    adapter context
 * @ps_config:  pointer to power save configure
 *
 * This function sets power save parameters.
 *
 * Return: 0 - success
 *    others - failure
 */
int wlan_hdd_set_power_save(struct hdd_adapter *adapter,
	struct p2p_ps_config *ps_config);

/**
 * wlan_hdd_set_mas() - Function to set MAS value to FW
 * @adapter:            Pointer to HDD adapter
 * @mas_value:          0-Disable, 1-Enable MAS
 *
 * This function passes down the value of MAS to FW
 *
 * Return: Configuration message posting status, SUCCESS or Fail
 *
 */
int32_t wlan_hdd_set_mas(struct hdd_adapter *adapter, uint8_t mas_value);

/**
 * wlan_hdd_set_mcc_p2p_quota() - Function to set quota for P2P
 * to FW
 * @adapter:            Pointer to HDD adapter
 * @set_value:          Quota value for the interface
 *
 * This function is used to set the quota for P2P cases
 *
 * Return: Configuration message posting status, SUCCESS or Fail
 *
 */
int wlan_hdd_set_mcc_p2p_quota(struct hdd_adapter *adapter,
			       uint32_t set_value);

/**
 * wlan_hdd_go_set_mcc_p2p_quota() - Function to set quota for
 * P2P GO to FW
 * @hostapd_adapter:    Pointer to HDD adapter
 * @set_value:          Quota value for the interface
 *
 * This function is used to set the quota for P2P GO cases
 *
 * Return: Configuration message posting status, SUCCESS or Fail
 *
 */
int wlan_hdd_go_set_mcc_p2p_quota(struct hdd_adapter *hostapd_adapter,
				  uint32_t set_value);
/**
 * wlan_hdd_set_mcc_latency() - Set MCC latency to FW
 * @adapter: Pointer to HDD adapter
 * @set_value: Latency value
 *
 * Sets the MCC latency value during STA-P2P concurrency
 *
 * Return: None
 */
void wlan_hdd_set_mcc_latency(struct hdd_adapter *adapter, int set_value);

/**
 * wlan_hdd_cleanup_actionframe() - Cleanup action frame
 * @adapter: Pointer to HDD adapter
 *
 * This function cleans up action frame.
 *
 * Return: None
 */
void wlan_hdd_cleanup_actionframe(struct hdd_adapter *adapter);
#endif /* __P2P_H */
