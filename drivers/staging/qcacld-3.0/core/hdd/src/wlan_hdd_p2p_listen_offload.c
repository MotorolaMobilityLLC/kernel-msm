/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_vendor_p2p_listen_offload.c
 *
 * WLAN p2p listen offload functions
 *
 */

#include "osif_sync.h"
#include <wlan_hdd_includes.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <wlan_hdd_p2p.h>
#include <wlan_p2p_ucfg_api.h>
#include <wlan_hdd_p2p_listen_offload.h>

/* P2P listen offload device types parameters length in bytes */
#define P2P_LO_MAX_REQ_DEV_TYPE_COUNT (10)
#define P2P_LO_WPS_DEV_TYPE_LEN (8)
#define P2P_LO_DEV_TYPE_MAX_LEN \
	(P2P_LO_MAX_REQ_DEV_TYPE_COUNT * P2P_LO_WPS_DEV_TYPE_LEN)

const struct nla_policy
p2p_listen_offload_policy[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_CHANNEL] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_PERIOD] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_INTERVAL] = {
							.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_COUNT] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_DEVICE_TYPES] = {
					.type = NLA_BINARY,
					.len = P2P_LO_DEV_TYPE_MAX_LEN },
	[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_VENDOR_IE] = {
					.type = NLA_BINARY,
					.len = MAX_GENIE_LEN },
	[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_CTRL_FLAG] = {
					.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_CHANNEL] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_STOP_REASON] = {
						.type = NLA_U8 },
};

/**
 * wlan_hdd_listen_offload_start() - hdd set listen offload start
 * @adapter: adapter context
 * @params: listen offload parameters
 *
 * This function sets listen offload start parameters.
 *
 * Return: 0 on success, others on failure
 */
static int wlan_hdd_listen_offload_start(struct hdd_adapter *adapter,
					 struct sir_p2p_lo_start *params)
{
	struct wlan_objmgr_psoc *psoc;
	struct p2p_lo_start lo_start;
	struct hdd_context *hdd_ctx;
	QDF_STATUS status;

	if (!adapter || !params) {
		hdd_err("null param, adapter:%pK, params:%pK",
			adapter, params);
		return -EINVAL;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	psoc = hdd_ctx->psoc;
	if (!psoc) {
		hdd_err("psoc is null");
		return -EINVAL;
	}

	lo_start.vdev_id = params->vdev_id;
	lo_start.ctl_flags = params->ctl_flags;
	lo_start.freq = params->freq;
	lo_start.period = params->period;
	lo_start.interval = params->interval;
	lo_start.count = params->count;
	lo_start.device_types = params->device_types;
	lo_start.dev_types_len = params->dev_types_len;
	lo_start.probe_resp_tmplt = params->probe_resp_tmplt;
	lo_start.probe_resp_len = params->probe_resp_len;

	status = ucfg_p2p_lo_start(psoc, &lo_start);
	hdd_debug("p2p listen offload start, status:%d", status);

	return qdf_status_to_os_return(status);
}

/**
 * __wlan_hdd_cfg80211_p2p_lo_start () - start P2P Listen Offload
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * This function is to process the p2p listen offload start vendor
 * command. It parses the input parameters and invoke WMA API to
 * send the command to firmware.
 *
 * Return: 0 on success, negative errno on failure
 */
static int __wlan_hdd_cfg80211_p2p_lo_start(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data,
					    int data_len)
{
	int ret;
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_MAX + 1];
	struct sir_p2p_lo_start params;

	hdd_enter_dev(dev);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (hdd_get_conparam() == QDF_GLOBAL_FTM_MODE) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	if ((adapter->device_mode != QDF_P2P_DEVICE_MODE) &&
	    (adapter->device_mode != QDF_P2P_CLIENT_MODE) &&
	    (adapter->device_mode != QDF_P2P_GO_MODE)) {
		hdd_err("Invalid device mode %d", adapter->device_mode);
		return -EINVAL;
	}

	if (wlan_cfg80211_nla_parse(tb,
				    QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_MAX,
				    data, data_len,
				    p2p_listen_offload_policy)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	memset(&params, 0, sizeof(params));

	if (!tb[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_CTRL_FLAG])
		params.ctl_flags = 1;  /* set to default value */
	else
		params.ctl_flags = nla_get_u32(tb
			[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_CTRL_FLAG]);

	if (!tb[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_CHANNEL] ||
	    !tb[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_PERIOD] ||
	    !tb[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_INTERVAL] ||
	    !tb[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_COUNT] ||
	    !tb[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_DEVICE_TYPES] ||
	    !tb[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_VENDOR_IE]) {
		hdd_err("Attribute parsing failed");
		return -EINVAL;
	}

	params.vdev_id = adapter->vdev_id;
	params.freq = nla_get_u32(tb
		[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_CHANNEL]);
	if ((params.freq != 2412) && (params.freq != 2437) &&
	    (params.freq != 2462)) {
		hdd_err("Invalid listening channel: %d", params.freq);
		return -EINVAL;
	}

	params.period = nla_get_u32(tb
		[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_PERIOD]);
	if (!((params.period > 0) && (params.period < UINT_MAX))) {
		hdd_err("Invalid period: %d", params.period);
		return -EINVAL;
	}

	params.interval = nla_get_u32(tb
		[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_INTERVAL]);
	if (!((params.interval > 0) && (params.interval < UINT_MAX))) {
		hdd_err("Invalid interval: %d", params.interval);
		return -EINVAL;
	}

	params.count = nla_get_u32(tb
		[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_COUNT]);
	if (!((params.count >= 0) && (params.count < UINT_MAX))) {
		hdd_err("Invalid count: %d", params.count);
		return -EINVAL;
	}

	params.device_types = nla_data(tb
		[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_DEVICE_TYPES]);
	if (!params.device_types) {
		hdd_err("Invalid device types");
		return -EINVAL;
	}

	params.dev_types_len = nla_len(tb
		[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_DEVICE_TYPES]);
	/* device type length has to be multiple of P2P_LO_WPS_DEV_TYPE_LEN */
	if (0 != (params.dev_types_len % P2P_LO_WPS_DEV_TYPE_LEN)) {
		hdd_err("Invalid device type length: %d", params.dev_types_len);
		return -EINVAL;
	}

	params.probe_resp_tmplt = nla_data(tb
		[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_VENDOR_IE]);
	if (!params.probe_resp_tmplt) {
		hdd_err("Invalid probe response template");
		return -EINVAL;
	}

	/*
	 * IEs minimum length should be 2 bytes: 1 byte for element id
	 * and 1 byte for element id length.
	 */
	params.probe_resp_len = nla_len(tb
		[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_VENDOR_IE]);
	if (params.probe_resp_len < MIN_GENIE_LEN) {
		hdd_err("Invalid probe resp template length: %d",
			params.probe_resp_len);
		return -EINVAL;
	}

	hdd_debug("P2P LO params: freq=%d, period=%d, interval=%d, count=%d",
		  params.freq, params.period, params.interval, params.count);

	return wlan_hdd_listen_offload_start(adapter, &params);
}

int wlan_hdd_cfg80211_p2p_lo_start(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   const void *data,
				   int data_len)
{
	struct osif_vdev_sync *vdev_sync;
	int errno;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_p2p_lo_start(wiphy, wdev,
						 data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * wlan_hdd_listen_offload_stop() - hdd set listen offload stop
 * @adapter: adapter context
 *
 * This function sets listen offload stop parameters.
 *
 * Return: 0 on success, others on failure
 */
static int wlan_hdd_listen_offload_stop(struct hdd_adapter *adapter)
{
	struct wlan_objmgr_psoc *psoc;
	struct hdd_context *hdd_ctx;
	uint32_t vdev_id;
	QDF_STATUS status;

	if (!adapter) {
		hdd_err("adapter is null, adapter:%pK", adapter);
		return -EINVAL;
	}

	vdev_id = (uint32_t)adapter->vdev_id;
	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	psoc = hdd_ctx->psoc;
	if (!psoc) {
		hdd_err("psoc is null");
		return -EINVAL;
	}

	status = ucfg_p2p_lo_stop(psoc, vdev_id);
	hdd_debug("p2p listen offload stop, status:%d", status);

	return qdf_status_to_os_return(status);
}

/**
 * __wlan_hdd_cfg80211_p2p_lo_stop () - stop P2P Listen Offload
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * This function is to process the p2p listen offload stop vendor
 * command. It invokes WMA API to send command to firmware.
 *
 * Return: 0 on success, negative errno on failure
 */
static int __wlan_hdd_cfg80211_p2p_lo_stop(struct wiphy *wiphy,
					   struct wireless_dev *wdev,
					   const void *data,
					   int data_len)
{
	struct hdd_adapter *adapter;
	struct net_device *dev = wdev->netdev;

	if (hdd_get_conparam() == QDF_GLOBAL_FTM_MODE) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	if ((adapter->device_mode != QDF_P2P_DEVICE_MODE) &&
	    (adapter->device_mode != QDF_P2P_CLIENT_MODE) &&
	    (adapter->device_mode != QDF_P2P_GO_MODE)) {
		hdd_err("Invalid device mode");
		return -EINVAL;
	}

	return wlan_hdd_listen_offload_stop(adapter);
}

int wlan_hdd_cfg80211_p2p_lo_stop(struct wiphy *wiphy,
				  struct wireless_dev *wdev,
				  const void *data,
				  int data_len)
{
	struct osif_vdev_sync *vdev_sync;
	int errno;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_p2p_lo_stop(wiphy, wdev,
						data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

