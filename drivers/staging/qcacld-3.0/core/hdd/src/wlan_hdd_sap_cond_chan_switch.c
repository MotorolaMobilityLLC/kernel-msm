/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_sap_cond_chan_switch.c
 *
 * WLAN SAP conditional channel switch functions
 *
 */

#include <cds_utils.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include "osif_sync.h"
#include <qdf_str.h>
#include <wlan_hdd_includes.h>
#include <wlan_hdd_sap_cond_chan_switch.h>

/* default pre cac channel bandwidth */
#define DEFAULT_PRE_CAC_BANDWIDTH CH_WIDTH_80MHZ

/**
 * wlan_hdd_set_pre_cac_status() - Set the pre cac status
 * @pre_cac_adapter: AP adapter used for pre cac
 * @status: Status (true or false)
 *
 * Sets the status of pre cac i.e., whether the pre cac is active or not
 *
 * Return: Zero on success, non-zero on failure
 */
static int wlan_hdd_set_pre_cac_status(struct hdd_adapter *pre_cac_adapter,
				       bool status)
{
	QDF_STATUS ret;

	ret = wlan_sap_set_pre_cac_status(
		WLAN_HDD_GET_SAP_CTX_PTR(pre_cac_adapter), status);
	if (QDF_IS_STATUS_ERROR(ret))
		return -EINVAL;

	return 0;
}

/**
 * wlan_hdd_set_chan_freq_before_pre_cac() - Save the channel before pre cac
 * @ap_adapter: AP adapter
 * @freq_before_pre_cac: Channel
 *
 * Saves the channel frequency which the AP was beaconing on before moving to
 * the pre cac channel. If radar is detected on the pre cac channel, this saved
 * channel will be used for AP operations.
 *
 * Return: Zero on success, non-zero on failure
 */
static int
wlan_hdd_set_chan_freq_before_pre_cac(struct hdd_adapter *ap_adapter,
				      qdf_freq_t freq_before_pre_cac)
{
	QDF_STATUS ret;
	struct sap_context *sap_ctx = WLAN_HDD_GET_SAP_CTX_PTR(ap_adapter);

	ret = wlan_sap_set_chan_freq_before_pre_cac(sap_ctx,
						    freq_before_pre_cac);
	if (QDF_IS_STATUS_ERROR(ret))
		return -EINVAL;

	return 0;
}

/**
 * wlan_hdd_validate_and_get_pre_cac_ch() - Validate and get pre cac channel
 * @hdd_ctx: HDD context
 * @ap_adapter: AP adapter
 * @chan_freq: Channel frequency requested by userspace
 * @pre_cac_chan_freq: Pointer to the pre CAC channel frequency storage
 *
 * Validates the channel provided by userspace. If user provided channel 0,
 * a valid outdoor channel must be selected from the regulatory channel.
 *
 * Return: Zero on success and non zero value on error
 */
static int wlan_hdd_validate_and_get_pre_cac_ch(struct hdd_context *hdd_ctx,
						struct hdd_adapter *ap_adapter,
						uint32_t chan_freq,
						uint32_t *pre_cac_chan_freq)
{
	uint32_t i;
	QDF_STATUS status;
	uint32_t weight_len = 0;
	uint32_t len = CFG_VALID_CHANNEL_LIST_LEN;
	uint32_t freq_list[NUM_CHANNELS] = {0};
	uint8_t pcl_weights[NUM_CHANNELS] = {0};
	mac_handle_t mac_handle;

	if (!chan_freq) {
		/* Channel is not obtained from PCL because PCL may not have
		 * the entire channel list. For example: if SAP is up on
		 * channel 6 and PCL is queried for the next SAP interface,
		 * if SCC is preferred, the PCL will contain only the channel
		 * 6. But, we are in need of a DFS channel. So, going with the
		 * first channel from the valid channel list.
		 */
		status = policy_mgr_get_valid_chans(hdd_ctx->psoc,
						    freq_list, &len);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("Failed to get channel list");
			return -EINVAL;
		}
		policy_mgr_update_with_safe_channel_list(hdd_ctx->psoc,
							 freq_list, &len,
							 pcl_weights,
							 weight_len);
		for (i = 0; i < len; i++) {
			if (wlan_reg_is_dfs_for_freq(hdd_ctx->pdev,
						     freq_list[i])) {
				*pre_cac_chan_freq = freq_list[i];
				break;
			}
		}
		if (*pre_cac_chan_freq == 0) {
			hdd_err("unable to find outdoor channel");
			return -EINVAL;
		}
	} else {
		/* Only when driver selects a channel, check is done for
		 * unnsafe and NOL channels. When user provides a fixed channel
		 * the user is expected to take care of this.
		 */
		mac_handle = hdd_ctx->mac_handle;
		if (!sme_is_channel_valid(mac_handle, chan_freq) ||
		    !wlan_reg_is_dfs_for_freq(hdd_ctx->pdev, chan_freq)) {
			hdd_err("Invalid channel for pre cac:%d", chan_freq);
			return -EINVAL;
		}
		*pre_cac_chan_freq = chan_freq;
	}
	hdd_debug("selected pre cac channel:%d", *pre_cac_chan_freq);
	return 0;
}

static int wlan_set_def_pre_cac_chan(struct hdd_context *hdd_ctx,
				     uint32_t pre_cac_ch_freq,
				     struct cfg80211_chan_def *chandef,
				     enum nl80211_channel_type *chantype,
				     enum phy_ch_width *ch_width)
{
	enum nl80211_channel_type channel_type;
	struct ieee80211_channel *ieee_chan;
	struct ch_params ch_params = {0};

	ieee_chan = ieee80211_get_channel(hdd_ctx->wiphy,
					  pre_cac_ch_freq);
	if (!ieee_chan) {
		hdd_err("channel converion failed %d", pre_cac_ch_freq);
		return -EINVAL;
	}
	ch_params.ch_width = *ch_width;
	wlan_reg_set_channel_params_for_freq(hdd_ctx->pdev,
					     pre_cac_ch_freq, 0,
					     &ch_params);
	switch (ch_params.sec_ch_offset) {
	case HIGH_PRIMARY_CH:
		channel_type = NL80211_CHAN_HT40MINUS;
		break;
	case LOW_PRIMARY_CH:
		channel_type = NL80211_CHAN_HT40PLUS;
		break;
	default:
		channel_type = NL80211_CHAN_HT20;
		break;
	}
	cfg80211_chandef_create(chandef, ieee_chan, channel_type);
	switch (ch_params.ch_width) {
	case CH_WIDTH_80MHZ:
		chandef->width = NL80211_CHAN_WIDTH_80;
		break;
	case CH_WIDTH_80P80MHZ:
		chandef->width = NL80211_CHAN_WIDTH_80P80;
		if (ch_params.mhz_freq_seg1)
			chandef->center_freq2 = ch_params.mhz_freq_seg1;
		break;
	case CH_WIDTH_160MHZ:
		chandef->width = NL80211_CHAN_WIDTH_160;
		break;
	default:
		break;
	}
	if (ch_params.ch_width == CH_WIDTH_80MHZ ||
	    ch_params.ch_width == CH_WIDTH_80P80MHZ ||
	    ch_params.ch_width == CH_WIDTH_160MHZ) {
		if (ch_params.mhz_freq_seg0)
			chandef->center_freq1 = ch_params.mhz_freq_seg0;
	}
	*chantype = channel_type;
	*ch_width = ch_params.ch_width;
	hdd_debug("pre cac ch def: chan:%d width:%d freq1:%d freq2:%d",
		  chandef->chan->center_freq, chandef->width,
		  chandef->center_freq1, chandef->center_freq2);

	return 0;
}
/**
 * __wlan_hdd_request_pre_cac() - Start pre CAC in the driver
 * @hdd_ctx: the HDD context to operate against
 * @chan_freq: Channel frequency option provided by userspace
 * @out_adapter: out parameter for the newly created pre-cac adapter
 *
 * Sets the driver to the required hardware mode and start an adapter for
 * pre CAC which will mimic an AP.
 *
 * Return: Zero on success, non-zero value on error
 */
static int __wlan_hdd_request_pre_cac(struct hdd_context *hdd_ctx,
				      uint32_t chan_freq,
				      struct hdd_adapter **out_adapter)
{
	uint8_t *mac_addr = NULL;
	uint32_t pre_cac_chan_freq = 0;
	int ret;
	struct hdd_adapter *ap_adapter, *pre_cac_adapter;
	struct hdd_ap_ctx *hdd_ap_ctx;
	QDF_STATUS status;
	struct wiphy *wiphy;
	struct net_device *dev;
	struct cfg80211_chan_def chandef;
	enum nl80211_channel_type channel_type;
	mac_handle_t mac_handle;
	bool val;
	enum phy_ch_width cac_ch_width;

	if (!policy_mgr_is_hw_dbs_capable(hdd_ctx->psoc)) {
		hdd_debug("Pre CAC is not supported on non-dbs platforms");
		return -EINVAL;
	}

	pre_cac_adapter = hdd_get_adapter_by_iface_name(hdd_ctx,
							SAP_PRE_CAC_IFNAME);
	if (pre_cac_adapter) {
		/* Flush existing pre_cac work */
		if (hdd_ctx->sap_pre_cac_work.fn)
			cds_flush_work(&hdd_ctx->sap_pre_cac_work);
	} else {
		if (policy_mgr_get_connection_count(hdd_ctx->psoc) > 1) {
			hdd_err("pre cac not allowed in concurrency");
			return -EINVAL;
		}
	}

	ap_adapter = hdd_get_adapter(hdd_ctx, QDF_SAP_MODE);
	if (!ap_adapter) {
		hdd_err("unable to get SAP adapter");
		return -EINVAL;
	}

	if (qdf_atomic_read(&ap_adapter->ch_switch_in_progress)) {
		hdd_err("pre cac not allowed during CSA");
		return -EINVAL;
	}

	mac_handle = hdd_ctx->mac_handle;
	val = wlan_sap_is_pre_cac_active(mac_handle);
	if (val) {
		hdd_err("pre cac is already in progress");
		return -EINVAL;
	}

	hdd_ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(ap_adapter);
	if (!hdd_ap_ctx) {
		hdd_err("SAP context is NULL");
		return -EINVAL;
	}

	if (wlan_reg_is_dfs_for_freq(hdd_ctx->pdev,
				     hdd_ap_ctx->operating_chan_freq)) {
		hdd_err("SAP is already on DFS channel:%d",
			hdd_ap_ctx->operating_chan_freq);
		return -EINVAL;
	}

	if (!WLAN_REG_IS_24GHZ_CH_FREQ(hdd_ap_ctx->operating_chan_freq)) {
		hdd_err("pre CAC alllowed only when SAP is in 2.4GHz:%d",
			hdd_ap_ctx->operating_chan_freq);
		return -EINVAL;
	}

	hdd_debug("channel: %d", chan_freq);

	ret = wlan_hdd_validate_and_get_pre_cac_ch(
		hdd_ctx, ap_adapter, chan_freq, &pre_cac_chan_freq);
	if (ret != 0) {
		hdd_err("can't validate pre-cac channel");
		goto release_intf_addr_and_return_failure;
	}

	hdd_debug("starting pre cac SAP  adapter");

	if (!pre_cac_adapter) {
		mac_addr = wlan_hdd_get_intf_addr(hdd_ctx, QDF_SAP_MODE);
		if (!mac_addr) {
			hdd_err("can't add virtual intf: Not getting valid mac addr");
			return -EINVAL;
		}

		/**
		 * Starting a SAP adapter:
		 * Instead of opening an adapter, we could just do a SME open
		 * session for AP type. But, start BSS would still need an
		 * adapter. So, this option is not taken.
		 *
		 * hdd open adapter is going to register this precac interface
		 * with user space. This interface though exposed to user space
		 * will be in DOWN state. Consideration was done to avoid this
		 * registration to the user space. But, as part of SAP
		 * operations multiple events are sent to user space. Some of
		 * these events received from unregistered interface was
		 * causing crashes. So, retaining the registration.
		 *
		 * So, this interface would remain registered and will remain
		 * in DOWN state for the CAC duration. We will add notes in the
		 * feature announcement to not use this temporary interface for
		 * any activity from user space.
		 */
		pre_cac_adapter = hdd_open_adapter(hdd_ctx, QDF_SAP_MODE,
						   SAP_PRE_CAC_IFNAME, mac_addr,
						   NET_NAME_UNKNOWN, true);

		if (!pre_cac_adapter) {
			hdd_err("error opening the pre cac adapter");
			goto release_intf_addr_and_return_failure;
		}
	}

	sap_clear_global_dfs_param(mac_handle,
				   WLAN_HDD_GET_SAP_CTX_PTR(pre_cac_adapter));

	/*
	 * This interface is internally created by the driver. So, no interface
	 * up comes for this interface from user space and hence starting
	 * the adapter internally.
	 */
	if (hdd_start_adapter(pre_cac_adapter)) {
		hdd_err("error starting the pre cac adapter");
		goto close_pre_cac_adapter;
	}

	hdd_debug("preparing for start ap/bss on the pre cac adapter");

	wiphy = hdd_ctx->wiphy;
	dev = pre_cac_adapter->dev;

	/* Since this is only a dummy interface lets us use the IEs from the
	 * other active SAP interface. In regular scenarios, these IEs would
	 * come from the user space entity
	 */
	pre_cac_adapter->session.ap.beacon = qdf_mem_malloc(
			sizeof(*ap_adapter->session.ap.beacon));
	if (!pre_cac_adapter->session.ap.beacon)
		goto stop_close_pre_cac_adapter;

	qdf_mem_copy(pre_cac_adapter->session.ap.beacon,
		     ap_adapter->session.ap.beacon,
		     sizeof(*pre_cac_adapter->session.ap.beacon));
	pre_cac_adapter->session.ap.sap_config.ch_width_orig =
			ap_adapter->session.ap.sap_config.ch_width_orig;
	pre_cac_adapter->session.ap.sap_config.authType =
			ap_adapter->session.ap.sap_config.authType;

	/* The orginal premise is that on moving from 2.4GHz to 5GHz, the SAP
	 * will continue to operate on the same bandwidth as that of the 2.4GHz
	 * operations. Only bandwidths 20MHz/40MHz are possible on 2.4GHz band.
	 * Now some customer request to start AP on higher BW such as 80Mhz.
	 * Hence use max possible supported BW based on phymode configurated
	 * on SAP.
	 */
	cac_ch_width = wlansap_get_max_bw_by_phymode(
			WLAN_HDD_GET_SAP_CTX_PTR(ap_adapter));
	if (cac_ch_width > DEFAULT_PRE_CAC_BANDWIDTH)
		cac_ch_width = DEFAULT_PRE_CAC_BANDWIDTH;

	qdf_mem_zero(&chandef, sizeof(struct cfg80211_chan_def));
	if (wlan_set_def_pre_cac_chan(hdd_ctx, pre_cac_chan_freq,
				      &chandef, &channel_type,
				      &cac_ch_width)) {
		hdd_err("error set pre_cac channel %d", pre_cac_chan_freq);
		goto close_pre_cac_adapter;
	}
	pre_cac_adapter->session.ap.sap_config.ch_width_orig = chandef.width;

	hdd_debug("existing ap phymode:%d pre cac ch_width:%d freq:%d",
		  ap_adapter->session.ap.sap_config.SapHw_mode,
		  cac_ch_width, pre_cac_chan_freq);
	/*
	 * Doing update after opening and starting pre-cac adapter will make
	 * sure that driver won't do hardware mode change if there are any
	 * initial hick-ups or issues in pre-cac adapter's configuration.
	 * Since current SAP is in 2.4GHz and pre CAC channel is in 5GHz, this
	 * connection update should result in DBS mode
	 */
	status = policy_mgr_update_and_wait_for_connection_update(
			hdd_ctx->psoc, ap_adapter->vdev_id, pre_cac_chan_freq,
			POLICY_MGR_UPDATE_REASON_PRE_CAC);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("error in moving to DBS mode");
		goto stop_close_pre_cac_adapter;
	}

	ret = wlan_hdd_set_channel(wiphy, dev, &chandef, channel_type);
	if (ret != 0) {
		hdd_err("failed to set channel");
		goto stop_close_pre_cac_adapter;
	}

	status = wlan_hdd_cfg80211_start_bss(pre_cac_adapter, NULL,
			PRE_CAC_SSID, qdf_str_len(PRE_CAC_SSID),
			NL80211_HIDDEN_SSID_NOT_IN_USE, false);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("start bss failed");
		goto stop_close_pre_cac_adapter;
	}

	/*
	 * The pre cac status is set here. But, it would not be reset explicitly
	 * anywhere, since after the pre cac success/failure, the pre cac
	 * adapter itself would be removed.
	 */
	ret = wlan_hdd_set_pre_cac_status(pre_cac_adapter, true);
	if (ret != 0) {
		hdd_err("failed to set pre cac status");
		goto stop_close_pre_cac_adapter;
	}

	ret = wlan_hdd_set_chan_freq_before_pre_cac(ap_adapter,
						    hdd_ap_ctx->operating_chan_freq);
	if (ret != 0) {
		hdd_err("failed to set channel before pre cac");
		goto stop_close_pre_cac_adapter;
	}

	ap_adapter->pre_cac_freq = pre_cac_chan_freq;
	pre_cac_adapter->is_pre_cac_adapter = true;

	*out_adapter = pre_cac_adapter;

	return 0;

stop_close_pre_cac_adapter:
	hdd_stop_adapter(hdd_ctx, pre_cac_adapter);
	qdf_mem_free(pre_cac_adapter->session.ap.beacon);
	pre_cac_adapter->session.ap.beacon = NULL;
close_pre_cac_adapter:
	hdd_close_adapter(hdd_ctx, pre_cac_adapter, false);
release_intf_addr_and_return_failure:
	/*
	 * Release the interface address as the adapter
	 * failed to start, if you don't release then next
	 * adapter which is trying to come wouldn't get valid
	 * mac address. Remember we have limited pool of mac addresses
	 */
	if (mac_addr)
		wlan_hdd_release_intf_addr(hdd_ctx, mac_addr);
	return -EINVAL;
}

static int
wlan_hdd_start_pre_cac_trans(struct hdd_context *hdd_ctx,
			     struct osif_vdev_sync **out_vdev_sync,
			     bool *is_vdev_sync_created)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_START_PRE_CAC_TRANS;
	int errno;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		if (!qdf_str_cmp(adapter->dev->name, SAP_PRE_CAC_IFNAME)) {
			errno = osif_vdev_sync_trans_start(adapter->dev,
							   out_vdev_sync);

			hdd_adapter_dev_put_debug(adapter, dbgid);
			if (next_adapter)
				hdd_adapter_dev_put_debug(next_adapter,
							  dbgid);
			return errno;

		}
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}

	errno = osif_vdev_sync_create_and_trans(hdd_ctx->parent_dev,
						out_vdev_sync);
	if (errno)
		return errno;

	*is_vdev_sync_created = true;
	return 0;
}

int wlan_hdd_request_pre_cac(struct hdd_context *hdd_ctx, uint32_t chan_freq)
{
	struct hdd_adapter *adapter;
	struct osif_vdev_sync *vdev_sync;
	int errno;
	bool is_vdev_sync_created = false;

	errno = wlan_hdd_start_pre_cac_trans(hdd_ctx, &vdev_sync,
					     &is_vdev_sync_created);
	if (errno)
		return errno;

	errno = __wlan_hdd_request_pre_cac(hdd_ctx, chan_freq, &adapter);
	if (errno)
		goto destroy_sync;

	if (is_vdev_sync_created)
		osif_vdev_sync_register(adapter->dev, vdev_sync);
	osif_vdev_sync_trans_stop(vdev_sync);

	return 0;

destroy_sync:
	osif_vdev_sync_trans_stop(vdev_sync);
	if (is_vdev_sync_created)
		osif_vdev_sync_destroy(vdev_sync);

	return errno;
}

const struct nla_policy conditional_chan_switch_policy[
		QCA_WLAN_VENDOR_ATTR_SAP_CONDITIONAL_CHAN_SWITCH_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SAP_CONDITIONAL_CHAN_SWITCH_FREQ_LIST] = {
				.type = NLA_BINARY },
	[QCA_WLAN_VENDOR_ATTR_SAP_CONDITIONAL_CHAN_SWITCH_STATUS] = {
				.type = NLA_U32 },
};

/**
 * __wlan_hdd_cfg80211_conditional_chan_switch() - Conditional channel switch
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Processes the conditional channel switch request and invokes the helper
 * APIs to process the channel switch request.
 *
 * Return: 0 on success, negative errno on failure
 */
static int
__wlan_hdd_cfg80211_conditional_chan_switch(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data,
					    int data_len)
{
	int ret;
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter;
	struct nlattr
		*tb[QCA_WLAN_VENDOR_ATTR_SAP_CONDITIONAL_CHAN_SWITCH_MAX + 1];
	uint32_t freq_len, i;
	uint32_t *freq;
	bool is_dfs_mode_enabled = false;

	hdd_enter_dev(dev);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (QDF_STATUS_SUCCESS != ucfg_mlme_get_dfs_master_capability(
				hdd_ctx->psoc, &is_dfs_mode_enabled)) {
		hdd_err("Failed to get dfs master capability");
		return -EINVAL;
	}

	if (!is_dfs_mode_enabled) {
		hdd_err("DFS master capability is not present in the driver");
		return -EINVAL;
	}

	if (hdd_get_conparam() == QDF_GLOBAL_FTM_MODE) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	if (adapter->device_mode != QDF_SAP_MODE) {
		hdd_err("Invalid device mode %d", adapter->device_mode);
		return -EINVAL;
	}

	/*
	 * audit note: it is ok to pass a NULL policy here since only
	 * one attribute is parsed which is array of frequencies and
	 * it is explicitly validated for both under read and over read
	 */
	if (wlan_cfg80211_nla_parse(tb,
			   QCA_WLAN_VENDOR_ATTR_SAP_CONDITIONAL_CHAN_SWITCH_MAX,
			   data, data_len, conditional_chan_switch_policy)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_SAP_CONDITIONAL_CHAN_SWITCH_FREQ_LIST]) {
		hdd_err("Frequency list is missing");
		return -EINVAL;
	}

	freq_len = nla_len(
		tb[QCA_WLAN_VENDOR_ATTR_SAP_CONDITIONAL_CHAN_SWITCH_FREQ_LIST])/
		sizeof(uint32_t);

	if (freq_len > NUM_CHANNELS) {
		hdd_err("insufficient space to hold channels");
		return -ENOMEM;
	}

	hdd_debug("freq_len=%d", freq_len);

	freq = nla_data(
		tb[QCA_WLAN_VENDOR_ATTR_SAP_CONDITIONAL_CHAN_SWITCH_FREQ_LIST]);

	for (i = 0; i < freq_len; i++)
		hdd_debug("freq[%d]=%d", i, freq[i]);

	/*
	 * The input frequency list from user space is designed to be a
	 * priority based frequency list. This is only to accommodate any
	 * future request. But, current requirement is only to perform CAC
	 * on a single channel. So, the first entry from the list is picked.
	 *
	 * If channel is zero, any channel in the available outdoor regulatory
	 * domain will be selected.
	 */
	ret = wlan_hdd_request_pre_cac(hdd_ctx, freq[0]);
	if (ret) {
		hdd_err("pre cac request failed with reason:%d", ret);
		return ret;
	}

	return 0;
}

int wlan_hdd_cfg80211_conditional_chan_switch(struct wiphy *wiphy,
					      struct wireless_dev *wdev,
					      const void *data,
					      int data_len)
{
	struct osif_vdev_sync *vdev_sync;
	int errno;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_conditional_chan_switch(wiphy, wdev,
							    data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

