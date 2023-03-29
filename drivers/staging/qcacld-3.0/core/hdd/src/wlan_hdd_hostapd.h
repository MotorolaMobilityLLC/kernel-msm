/*
 * Copyright (c) 2013-2021 The Linux Foundation. All rights reserved.
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

#if !defined(WLAN_HDD_HOSTAPD_H)
#define WLAN_HDD_HOSTAPD_H

/**
 * DOC: wlan_hdd_hostapd.h
 *
 * WLAN Host Device driver hostapd header file
 */

/* Include files */

#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <qdf_list.h>
#include <qdf_types.h>
#include <wlan_hdd_main.h>

/* Preprocessor definitions and constants */

struct hdd_adapter *hdd_wlan_create_ap_dev(struct hdd_context *hdd_ctx,
				      tSirMacAddr macAddr,
				      unsigned char name_assign_type,
				      uint8_t *name);

enum csr_akm_type
hdd_translate_rsn_to_csr_auth_type(uint8_t auth_suite[4]);

/**
 * hdd_softap_set_channel_change() -
 * This function to support SAP channel change with CSA IE
 * set in the beacons.
 *
 * @dev: pointer to the net device.
 * @target_chan_freq: target channel frequency.
 * @target_bw: Target bandwidth to move.
 * If no bandwidth is specified, the value is CH_WIDTH_MAX
 * @forced: Force to switch channel, ignore SCC/MCC check
 *
 * Return: 0 for success, non zero for failure
 */
int hdd_softap_set_channel_change(struct net_device *dev,
					int target_chan_freq,
					enum phy_ch_width target_bw,
					bool forced);

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
/**
 * hdd_sap_restart_with_channel_switch() - SAP channel change with E/CSA
 * @ap_adapter: HDD adapter
 * @target_chan_freq: Channel frequency to which switch must happen
 * @target_bw: Bandwidth of the target channel
 * @forced: Force to switch channel, ignore SCC/MCC check
 *
 * Invokes the necessary API to perform channel switch for the SAP or GO
 *
 * Return: None
 */
void hdd_sap_restart_with_channel_switch(struct hdd_adapter *adapter,
				uint32_t target_chan_freq,
				uint32_t target_bw,
				bool forced);
/**
 * hdd_sap_restart_chan_switch_cb() - Function to restart SAP with
 * a different channel
 * @psoc: PSOC object information
 * @vdev_id: vdev id
 * @ch_freq: channel to switch
 * @forced: Force to switch channel, ignore SCC/MCC check
 *
 * This function restarts SAP with a different channel
 *
 * Return: None
 *
 */
void hdd_sap_restart_chan_switch_cb(struct wlan_objmgr_psoc *psoc,
				    uint8_t vdev_id, uint32_t ch_freq,
				    uint32_t channel_bw,
				    bool forced);
/**
 * wlan_hdd_get_channel_for_sap_restart() - Function to get
 * suitable channel and restart SAP
 * @psoc: PSOC object information
 * @vdev_id: vdev id
 * @ch_freq: channel to be returned
 *
 * This function gets the channel parameters to restart SAP
 *
 * Return: None
 *
 */
QDF_STATUS wlan_hdd_get_channel_for_sap_restart(
				struct wlan_objmgr_psoc *psoc,
				uint8_t vdev_id, uint32_t *ch_freq);

/**
 * wlan_get_ap_prefer_conc_ch_params() - Get prefer sap target channel
 *  bw parameters
 * @psoc: pointer to psoc
 * @vdev_id: vdev id
 * @chan_freq: sap channel
 * @ch_params: output channel parameters
 *
 * This function is used to get prefer sap target channel bw during sap force
 * scc CSA. The new bw will not exceed the orginal bw during start ap
 * request.
 *
 * Return: QDF_STATUS_SUCCESS if successfully
 */
QDF_STATUS
wlan_get_ap_prefer_conc_ch_params(
		struct wlan_objmgr_psoc *psoc,
		uint8_t vdev_id, uint32_t chan_freq,
		struct ch_params *ch_params);

/**
 * hdd_get_ap_6ghz_capable() - Get ap vdev 6ghz capable flags
 * @psoc: PSOC object information
 * @vdev_id: vdev id
 *
 * This function gets 6ghz capable information based on hdd ap adapter
 * context.
 *
 * Return: uint32_t, vdev 6g capable flags from enum conn_6ghz_flag
 */
uint32_t hdd_get_ap_6ghz_capable(struct wlan_objmgr_psoc *psoc,
				 uint8_t vdev_id);
#endif

/**
 * wlan_hdd_set_sap_csa_reason() - Function to set
 * sap csa reason
 * @psoc: PSOC object information
 * @vdev_id: vdev id
 * @reason: reason to be updated
 *
 * This function sets the reason for SAP channel switch
 *
 * Return: None
 *
 */
void wlan_hdd_set_sap_csa_reason(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
				 uint8_t reason);
eCsrEncryptionType
hdd_translate_rsn_to_csr_encryption_type(uint8_t cipher_suite[4]);

eCsrEncryptionType
hdd_translate_rsn_to_csr_encryption_type(uint8_t cipher_suite[4]);

enum csr_akm_type
hdd_translate_wpa_to_csr_auth_type(uint8_t auth_suite[4]);

eCsrEncryptionType
hdd_translate_wpa_to_csr_encryption_type(uint8_t cipher_suite[4]);

QDF_STATUS hdd_softap_sta_deauth(struct hdd_adapter *adapter,
				 struct csr_del_sta_params *param);
void hdd_softap_sta_disassoc(struct hdd_adapter *adapter,
			     struct csr_del_sta_params *param);

QDF_STATUS hdd_hostapd_sap_event_cb(struct sap_event *sap_event,
				    void *context);
/**
 * hdd_init_ap_mode() - to init the AP adaptor
 * @adapter: SAP/GO adapter
 * @rtnl_held: flag to indicate if RTNL lock needs to be acquired
 *
 * This API can be called to open the SAP session as well as
 * to create and store the vdev object. It also initializes necessary
 * SAP adapter related params.
 */
QDF_STATUS hdd_init_ap_mode(struct hdd_adapter *adapter, bool reinit);
/**
 * hdd_deinit_ap_mode() - to deinit the AP adaptor
 * @hdd_ctx: pointer to hdd_ctx
 * @adapter: SAP/GO adapter
 * @rtnl_held: flag to indicate if RTNL lock needs to be acquired
 *
 * This API can be called to close the SAP session as well as
 * release the vdev object completely. It also deinitializes necessary
 * SAP adapter related params.
 */
void hdd_deinit_ap_mode(struct hdd_context *hdd_ctx,
			struct hdd_adapter *adapter, bool rtnl_held);
void hdd_set_ap_ops(struct net_device *dev);
/**
 * hdd_sap_create_ctx() - Wrapper API to create SAP context
 * @adapter: pointer to adapter
 *
 * This wrapper API can be called to create the sap context. It will
 * eventually calls SAP API to create the sap context
 *
 * Return: true or false based on overall success or failure
 */
bool hdd_sap_create_ctx(struct hdd_adapter *adapter);
/**
 * hdd_sap_destroy_ctx() - Wrapper API to destroy SAP context
 * @adapter: pointer to adapter
 *
 * This wrapper API can be called to destroy the sap context. It will
 * eventually calls SAP API to destroy the sap context
 *
 * Return: true or false based on overall success or failure
 */
bool hdd_sap_destroy_ctx(struct hdd_adapter *adapter);
/**
 * hdd_sap_destroy_ctx_all() - Wrapper API to destroy all SAP context
 * @adapter: pointer to adapter
 * @is_ssr: true if SSR is in progress
 *
 * This wrapper API can be called to destroy all the sap context.
 * if is_ssr is true, it will return as sap_ctx will be used when
 * restart sap.
 *
 * Return: none
 */
void hdd_sap_destroy_ctx_all(struct hdd_context *hdd_ctx, bool is_ssr);

/**
 * hdd_hostapd_stop_no_trans() - hdd stop function for hostapd interface
 * @dev: pointer to net_device structure
 *
 * This is called in response to ifconfig down. Vdev sync transaction
 * should be started before calling this API.
 *
 * Return - 0 for success non-zero for failure
 */
int hdd_hostapd_stop_no_trans(struct net_device *dev);

int hdd_hostapd_stop(struct net_device *dev);
int hdd_sap_context_init(struct hdd_context *hdd_ctx);
void hdd_sap_context_destroy(struct hdd_context *hdd_ctx);
#ifdef QCA_HT_2040_COEX
QDF_STATUS hdd_set_sap_ht2040_mode(struct hdd_adapter *adapter,
				   uint8_t channel_type);
#endif

int wlan_hdd_cfg80211_stop_ap(struct wiphy *wiphy,
			      struct net_device *dev);

int wlan_hdd_cfg80211_start_ap(struct wiphy *wiphy,
			       struct net_device *dev,
			       struct cfg80211_ap_settings *params);

int wlan_hdd_cfg80211_change_beacon(struct wiphy *wiphy,
				    struct net_device *dev,
				    struct cfg80211_beacon_data *params);

/**
 * hdd_is_peer_associated - is peer connected to softap
 * @adapter: pointer to softap adapter
 * @mac_addr: address to check in peer list
 *
 * This function has to be invoked only when bss is started and is used
 * to check whether station with specified addr is peer or not
 *
 * Return: true if peer mac, else false
 */
bool hdd_is_peer_associated(struct hdd_adapter *adapter,
			    struct qdf_mac_addr *mac_addr);

int hdd_destroy_acs_timer(struct hdd_adapter *adapter);

QDF_STATUS wlan_hdd_config_acs(struct hdd_context *hdd_ctx,
			       struct hdd_adapter *adapter);

void hdd_sap_indicate_disconnect_for_sta(struct hdd_adapter *adapter);

/**
 * wlan_hdd_disable_channels() - Cache the channels
 * and current state of the channels from the channel list
 * received in the command and disable the channels on the
 * wiphy and reg table.
 * @hdd_ctx: Pointer to hdd context
 *
 * Return: 0 on success, Error code on failure
 */
int wlan_hdd_disable_channels(struct hdd_context *hdd_ctx);

/*
 * hdd_check_and_disconnect_sta_on_invalid_channel() - Disconnect STA if it is
 * on invalid channel
 * @hdd_ctx: pointer to hdd context
 * @reason: Mac Disconnect reason code as per @enum wlan_reason_code
 *
 * STA should be disconnected before starting the SAP if it is on indoor
 * channel.
 *
 * Return: void
 */
void
hdd_check_and_disconnect_sta_on_invalid_channel(struct hdd_context *hdd_ctx,
						enum wlan_reason_code reason);

/**
 * hdd_convert_dot11mode_from_phymode() - get dot11 mode from phymode
 * @phymode: phymode of sta associated to SAP
 *
 * The function is to convert the phymode to corresponding dot11 mode
 *
 * Return: dot11mode.
 */
enum qca_wlan_802_11_mode hdd_convert_dot11mode_from_phymode(int phymode);

/**
 * hdd_stop_sap_due_to_invalid_channel() - to stop sap in case of invalid chnl
 * @work: pointer to work structure
 *
 * Let's say SAP detected RADAR and trying to select the new channel and if no
 * valid channel is found due to none of the channels are available or
 * regulatory restriction then SAP needs to be stopped. so SAP state-machine
 * will create a work to stop the bss
 *
 * stop bss has to happen through worker thread because radar indication comes
 * from FW through mc thread or main host thread and if same thread is used to
 * do stopbss then waiting for stopbss to finish operation will halt mc thread
 * to freeze which will trigger stopbss timeout. Instead worker thread can do
 * the stopbss operation while mc thread waits for stopbss to finish.
 *
 * Return: none
 */
void hdd_stop_sap_due_to_invalid_channel(struct work_struct *work);

/**
 * hdd_is_any_sta_connecting() - check if any sta is connecting
 * @hdd_ctx: hdd context
 *
 * Return: true if any sta is connecting
 */
bool hdd_is_any_sta_connecting(struct hdd_context *hdd_ctx);
#endif /* end #if !defined(WLAN_HDD_HOSTAPD_H) */
